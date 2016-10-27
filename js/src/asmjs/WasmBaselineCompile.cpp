/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

/* WebAssembly baseline compiler ("RabaldrMonkey")
 *
 * General status notes:
 *
 * "FIXME" indicates a known or suspected bug.
 *
 * "TODO" indicates an opportunity for a general improvement, with an
 * additional tag to indicate the area of improvement.  These are
 * generally not filed as bugs.
 *
 * Unimplemented functionality:
 *
 *  - Tiered compilation (bug 1277562)
 *  - profiler support / devtools (bug 1286948)
 *  - SIMD
 *  - Atomics
 *
 * There are lots of machine dependencies here but they are pretty
 * well isolated to a segment of the compiler.  Many dependencies
 * will eventually be factored into the MacroAssembler layer and shared
 * with other code generators.
 *
 *
 * High-value compiler performance improvements:
 *
 * - The specific-register allocator (the needI32(r), needI64(r) etc
 *   methods) can avoid syncing the value stack if the specific
 *   register is in use but there is a free register to shuffle the
 *   specific register into.  (This will also improve the generated
 *   code.)  The sync happens often enough here to show up in
 *   profiles, because it is triggered by integer multiply and divide.
 *
 *
 * High-value code generation improvements:
 *
 * - Many opportunities for cheaply folding in a constant rhs, we do
 *   this already for I32 add and shift operators, this reduces
 *   register pressure and instruction count.
 *
 * - Boolean evaluation for control can be optimized by pushing a
 *   bool-generating operation onto the value stack in the same way
 *   that we now push latent constants and local lookups, or (easier)
 *   by remembering the operation in a side location if the next Expr
 *   will consume it.
 *
 * - Conditional branches (br_if and br_table) pessimize by branching
 *   over code that performs stack cleanup and a branch.  But if no
 *   cleanup is needed we could just branch conditionally to the
 *   target.
 *
 * - Register management around calls: At the moment we sync the value
 *   stack unconditionally (this is simple) but there are probably
 *   many common cases where we could instead save/restore live
 *   caller-saves registers and perform parallel assignment into
 *   argument registers.  This may be important if we keep some locals
 *   in registers.
 *
 * - Allocate some locals to registers on machines where there are
 *   enough registers.  This is probably hard to do well in a one-pass
 *   compiler but it might be that just keeping register arguments and
 *   the first few locals in registers is a viable strategy; another
 *   (more general) strategy is caching locals in registers in
 *   straight-line code.  Such caching could also track constant
 *   values in registers, if that is deemed valuable.  A combination
 *   of techniques may be desirable: parameters and the first few
 *   locals could be cached on entry to the function but not
 *   statically assigned to registers throughout.
 *
 *   (On a large corpus of code it should be possible to compute, for
 *   every signature comprising the types of parameters and locals,
 *   and using a static weight for loops, a list in priority order of
 *   which parameters and locals that should be assigned to registers.
 *   Or something like that.  Wasm makes this simple.  Static
 *   assignments are desirable because they are not flushed to memory
 *   by the pre-block sync() call.)
 */

#include "asmjs/WasmBaselineCompile.h"

#include "mozilla/MathAlgorithms.h"

#include "asmjs/WasmBinaryFormat.h"
#include "asmjs/WasmBinaryIterator.h"
#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmSignalHandlers.h"
#include "jit/AtomicOp.h"
#include "jit/IonTypes.h"
#include "jit/JitAllocPolicy.h"
#include "jit/Label.h"
#include "jit/MacroAssembler.h"
#include "jit/MIR.h"
#include "jit/Registers.h"
#include "jit/RegisterSets.h"
#if defined(JS_CODEGEN_ARM)
# include "jit/arm/Assembler-arm.h"
#endif
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
# include "jit/x86-shared/Architecture-x86-shared.h"
# include "jit/x86-shared/Assembler-x86-shared.h"
#endif

#include "jit/MacroAssembler-inl.h"

using mozilla::DebugOnly;
using mozilla::FloatingPoint;
using mozilla::IsPowerOfTwo;
using mozilla::SpecificNaN;

namespace js {
namespace wasm {

using namespace js::jit;
using JS::GenericNaN;

struct BaseCompilePolicy : ExprIterPolicy
{
    static const bool Output = true;

    // The baseline compiler tracks values on a stack of its own -- it
    // needs to scan that stack for spilling -- and thus has no need
    // for the values maintained by the iterator.
    //
    // The baseline compiler tracks control items on a stack of its
    // own as well.
    //
    // TODO / REDUNDANT: It would be nice if we could make use of the
    // iterator's ControlItems and not require our own stack for that.
};

typedef ExprIter<BaseCompilePolicy> BaseExprIter;

typedef bool IsUnsigned;
typedef bool IsSigned;
typedef bool ZeroOnOverflow;
typedef bool IsKnownNotZero;
typedef bool HandleNaNSpecially;
typedef unsigned ByteSize;
typedef unsigned BitSize;

// UseABI::Wasm implies that the Tls/Heap/Global registers are nonvolatile,
// except when InterModule::True is also set, when they are volatile.
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

enum class UseABI { Wasm, System };
enum class InterModule { False = false, True = true };

#ifdef JS_CODEGEN_ARM64
// FIXME: This is not correct, indeed for ARM64 there is no reliable
// StackPointer and we'll need to change the abstractions that use
// SP-relative addressing.  There's a guard in emitFunction() below to
// prevent this workaround from having any consequence.  This hack
// exists only as a stopgap; there is no ARM64 JIT support yet.
static const Register StackPointer = RealStackPointer;
#endif

#ifdef JS_CODEGEN_X86
// The selection of EBX here steps gingerly around: the need for EDX
// to be allocatable for multiply/divide; ECX to be allocatable for
// shift/rotate; EAX (= ReturnReg) to be allocatable as the joinreg;
// EBX not being one of the WasmTableCall registers; and needing a
// temp register for load/store that has a single-byte persona.
static const Register ScratchRegX86 = ebx;

# define QUOT_REM_I64_CALLOUT
#endif

#ifdef JS_CODEGEN_ARM
// We need a temp for funcPtrCall.  It can't be any of the
// WasmTableCall registers, an argument register, or a scratch
// register, and probably should not be ReturnReg.
static const Register FuncPtrCallTemp = CallTempReg1;

// We use our own scratch register, because the macro assembler uses
// the regular scratch register(s) pretty liberally.  We could
// work around that in several cases but the mess does not seem
// worth it yet.  CallTempReg2 seems safe.
static const Register ScratchRegARM = CallTempReg2;

# define QUOT_REM_I64_CALLOUT
# define I64_TO_FLOAT_CALLOUT
# define FLOAT_TO_I64_CALLOUT
#endif

class BaseCompiler
{
    // We define our own ScratchRegister abstractions, deferring to
    // the platform's when possible.

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    typedef ScratchDoubleScope ScratchF64;
#else
    class ScratchF64
    {
      public:
        ScratchF64(BaseCompiler& b) {}
        operator FloatRegister() const {
            MOZ_CRASH("BaseCompiler platform hook - ScratchF64");
        }
    };
#endif

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    typedef ScratchFloat32Scope ScratchF32;
#else
    class ScratchF32
    {
      public:
        ScratchF32(BaseCompiler& b) {}
        operator FloatRegister() const {
            MOZ_CRASH("BaseCompiler platform hook - ScratchF32");
        }
    };
#endif

#if defined(JS_CODEGEN_X64)
    typedef ScratchRegisterScope ScratchI32;
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    class ScratchI32
    {
# ifdef DEBUG
        BaseCompiler& bc;
      public:
        ScratchI32(BaseCompiler& bc) : bc(bc) {
            MOZ_ASSERT(!bc.scratchRegisterTaken());
            bc.setScratchRegisterTaken(true);
        }
        ~ScratchI32() {
            MOZ_ASSERT(bc.scratchRegisterTaken());
            bc.setScratchRegisterTaken(false);
        }
# else
      public:
        ScratchI32(BaseCompiler& bc) {}
# endif
        operator Register() const {
# ifdef JS_CODEGEN_X86
            return ScratchRegX86;
# else
            return ScratchRegARM;
# endif
        }
    };
#else
    class ScratchI32
    {
      public:
        ScratchI32(BaseCompiler& bc) {}
        operator Register() const {
            MOZ_CRASH("BaseCompiler platform hook - ScratchI32");
        }
    };
#endif

    // A Label in the code, allocated out of a temp pool in the
    // TempAllocator attached to the compilation.

    struct PooledLabel : public Label, public TempObject, public InlineListNode<PooledLabel>
    {
        PooledLabel() : f(nullptr) {}
        explicit PooledLabel(BaseCompiler* f) : f(f) {}
        BaseCompiler* f;
    };

    typedef Vector<PooledLabel*, 8, SystemAllocPolicy> LabelVector;

    struct UniquePooledLabelFreePolicy
    {
        void operator()(PooledLabel* p) {
            p->f->freeLabel(p);
        }
    };

    typedef UniquePtr<PooledLabel, UniquePooledLabelFreePolicy> UniquePooledLabel;

    // The strongly typed register wrappers have saved my bacon a few
    // times; though they are largely redundant they stay, for now.

    // TODO / INVESTIGATE: Things would probably be simpler if these
    // inherited from Register, Register64, and FloatRegister.

    struct RegI32
    {
        RegI32() : reg(Register::Invalid()) {}
        explicit RegI32(Register reg) : reg(reg) {}
        Register reg;
        bool operator==(const RegI32& that) { return reg == that.reg; }
        bool operator!=(const RegI32& that) { return reg != that.reg; }
    };

    struct RegI64
    {
        RegI64() : reg(Register64::Invalid()) {}
        explicit RegI64(Register64 reg) : reg(reg) {}
        Register64 reg;
        bool operator==(const RegI64& that) { return reg == that.reg; }
        bool operator!=(const RegI64& that) { return reg != that.reg; }
    };

    struct RegF32
    {
        RegF32() {}
        explicit RegF32(FloatRegister reg) : reg(reg) {}
        FloatRegister reg;
        bool operator==(const RegF32& that) { return reg == that.reg; }
        bool operator!=(const RegF32& that) { return reg != that.reg; }
    };

    struct RegF64
    {
        RegF64() {}
        explicit RegF64(FloatRegister reg) : reg(reg) {}
        FloatRegister reg;
        bool operator==(const RegF64& that) { return reg == that.reg; }
        bool operator!=(const RegF64& that) { return reg != that.reg; }
    };

    struct AnyReg
    {
        AnyReg() { tag = NONE; }
        explicit AnyReg(RegI32 r) { tag = I32; i32_ = r; }
        explicit AnyReg(RegI64 r) { tag = I64; i64_ = r; }
        explicit AnyReg(RegF32 r) { tag = F32; f32_ = r; }
        explicit AnyReg(RegF64 r) { tag = F64; f64_ = r; }

        RegI32 i32() {
            MOZ_ASSERT(tag == I32);
            return i32_;
        }
        RegI64 i64() {
            MOZ_ASSERT(tag == I64);
            return i64_;
        }
        RegF32 f32() {
            MOZ_ASSERT(tag == F32);
            return f32_;
        }
        RegF64 f64() {
            MOZ_ASSERT(tag == F64);
            return f64_;
        }
        AnyRegister any() {
            switch (tag) {
              case F32: return AnyRegister(f32_.reg);
              case F64: return AnyRegister(f64_.reg);
              case I32: return AnyRegister(i32_.reg);
              case I64:
#ifdef JS_PUNBOX64
                return AnyRegister(i64_.reg.reg);
#else
                // The compiler is written so that this is never needed: any() is called
                // on arbitrary registers for asm.js but asm.js does not have 64-bit ints.
                // For wasm, any() is called on arbitrary registers only on 64-bit platforms.
                MOZ_CRASH("AnyReg::any() on 32-bit platform");
#endif
              case NONE:
                MOZ_CRASH("AnyReg::any() on NONE");
            }
            // Work around GCC 5 analysis/warning bug.
            MOZ_CRASH("AnyReg::any(): impossible case");
        }

        union {
            RegI32 i32_;
            RegI64 i64_;
            RegF32 f32_;
            RegF64 f64_;
        };
        enum { NONE, I32, I64, F32, F64 } tag;
    };

    struct Local
    {
        Local() : type_(MIRType::None), offs_(UINT32_MAX) {}
        Local(MIRType type, uint32_t offs) : type_(type), offs_(offs) {}

        void init(MIRType type_, uint32_t offs_) {
            this->type_ = type_;
            this->offs_ = offs_;
        }

        MIRType  type_;              // Type of the value, or MIRType::None
        uint32_t offs_;              // Zero-based frame offset of value, or UINT32_MAX

        MIRType type() const { MOZ_ASSERT(type_ != MIRType::None); return type_; }
        uint32_t offs() const { MOZ_ASSERT(offs_ != UINT32_MAX); return offs_; }
    };

    // Control node, representing labels and stack heights at join points.

    struct Control
    {
        Control(uint32_t framePushed, uint32_t stackSize)
            : label(nullptr),
              otherLabel(nullptr),
              framePushed(framePushed),
              stackSize(stackSize),
              deadOnArrival(false),
              deadThenBranch(false)
        {}

        PooledLabel* label;
        PooledLabel* otherLabel;        // Used for the "else" branch of if-then-else
        uint32_t framePushed;           // From masm
        uint32_t stackSize;             // Value stack height
        bool deadOnArrival;             // deadCode_ was set on entry to the region
        bool deadThenBranch;            // deadCode_ was set on exit from "then"
    };

    // Volatile registers except ReturnReg.

    static LiveRegisterSet VolatileReturnGPR;

    // The baseline compiler will use OOL code more sparingly than
    // Baldr since our code is not high performance and frills like
    // code density and branch prediction friendliness will be less
    // important.

    class OutOfLineCode : public TempObject
    {
      private:
        Label entry_;
        Label rejoin_;
        uint32_t framePushed_;

      public:
        OutOfLineCode() : framePushed_(UINT32_MAX) {}

        Label* entry() { return &entry_; }
        Label* rejoin() { return &rejoin_; }

        void setFramePushed(uint32_t framePushed) {
            MOZ_ASSERT(framePushed_ == UINT32_MAX);
            framePushed_ = framePushed;
        }

        void bind(MacroAssembler& masm) {
            MOZ_ASSERT(framePushed_ != UINT32_MAX);
            masm.bind(&entry_);
            masm.setFramePushed(framePushed_);
        }

        // Save volatile registers but not ReturnReg.

        void saveVolatileReturnGPR(MacroAssembler& masm) {
            masm.PushRegsInMask(BaseCompiler::VolatileReturnGPR);
        }

        // Restore volatile registers but not ReturnReg.

        void restoreVolatileReturnGPR(MacroAssembler& masm) {
            masm.PopRegsInMask(BaseCompiler::VolatileReturnGPR);
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

        virtual void generate(MacroAssembler& masm) = 0;
    };

    const ModuleGeneratorData&  mg_;
    BaseExprIter                iter_;
    const FuncBytes&            func_;
    size_t                      lastReadCallSite_;
    TempAllocator&              alloc_;
    const ValTypeVector&        locals_;         // Types of parameters and locals
    int32_t                     localSize_;      // Size of local area in bytes (stable after beginFunction)
    int32_t                     varLow_;         // Low byte offset of local area for true locals (not parameters)
    int32_t                     varHigh_;        // High byte offset + 1 of local area for true locals
    int32_t                     maxFramePushed_; // Max value of masm.framePushed() observed
    bool                        deadCode_;       // Flag indicating we should decode & discard the opcode
    ValTypeVector               SigI64I64_;
    ValTypeVector               SigDD_;
    ValTypeVector               SigD_;
    ValTypeVector               SigF_;
    ValTypeVector               SigI_;
    ValTypeVector               Sig_;
    Label                       returnLabel_;
    Label                       outOfLinePrologue_;
    Label                       bodyLabel_;
    TrapOffset                  prologueTrapOffset_;

    FuncCompileResults&         compileResults_;
    MacroAssembler&             masm;            // No '_' suffix - too tedious...

    AllocatableGeneralRegisterSet availGPR_;
    AllocatableFloatRegisterSet availFPU_;
#ifdef DEBUG
    bool                        scratchRegisterTaken_;
#endif

    TempObjectPool<PooledLabel> labelPool_;

    Vector<Local, 8, SystemAllocPolicy> localInfo_;
    Vector<OutOfLineCode*, 8, SystemAllocPolicy> outOfLine_;

    // Index into localInfo_ of the special local used for saving the TLS
    // pointer. This follows the function's real arguments and locals.
    uint32_t                    tlsSlot_;

    // On specific platforms we sometimes need to use specific registers.

#ifdef JS_CODEGEN_X64
    RegI64 specific_rax;
    RegI64 specific_rcx;
    RegI64 specific_rdx;
#endif

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
    RegI32 specific_eax;
    RegI32 specific_ecx;
    RegI32 specific_edx;
#endif

#if defined(JS_CODEGEN_X86)
    AllocatableGeneralRegisterSet singleByteRegs_;
#endif
#if defined(JS_NUNBOX32)
    RegI64 abiReturnRegI64;
#endif

    // The join registers are used to carry values out of blocks.
    // JoinRegI32 and joinRegI64 must overlap: emitBrIf and
    // emitBrTable assume that.

    RegI32 joinRegI32;
    RegI64 joinRegI64;
    RegF32 joinRegF32;
    RegF64 joinRegF64;

    // More members: see the stk_ and ctl_ vectors, defined below.

  public:
    BaseCompiler(const ModuleGeneratorData& mg,
                 Decoder& decoder,
                 const FuncBytes& func,
                 const ValTypeVector& locals,
                 FuncCompileResults& compileResults);

    MOZ_MUST_USE bool init();

    void finish();

    MOZ_MUST_USE bool emitFunction();

    // Used by some of the ScratchRegister implementations.
    operator MacroAssembler&() const { return masm; }

#ifdef DEBUG
    bool scratchRegisterTaken() const {
        return scratchRegisterTaken_;
    }
    void setScratchRegisterTaken(bool state) {
        scratchRegisterTaken_ = state;
    }
#endif

  private:

    ////////////////////////////////////////////////////////////
    //
    // Out of line code management.

    MOZ_MUST_USE OutOfLineCode* addOutOfLineCode(OutOfLineCode* ool) {
        if (!ool || !outOfLine_.append(ool))
            return nullptr;
        ool->setFramePushed(masm.framePushed());
        return ool;
    }

    MOZ_MUST_USE bool generateOutOfLineCode() {
        for (uint32_t i = 0; i < outOfLine_.length(); i++) {
            OutOfLineCode* ool = outOfLine_[i];
            ool->bind(masm);
            ool->generate(masm);
        }

        return !masm.oom();
    }

    ////////////////////////////////////////////////////////////
    //
    // The stack frame.

    // SP-relative load and store.

    int32_t localOffsetToSPOffset(int32_t offset) {
        return masm.framePushed() - offset;
    }

    void storeToFrameI32(Register r, int32_t offset) {
        masm.store32(r, Address(StackPointer, localOffsetToSPOffset(offset)));
    }

    void storeToFrameI64(Register64 r, int32_t offset) {
        masm.store64(r, Address(StackPointer, localOffsetToSPOffset(offset)));
    }

    void storeToFramePtr(Register r, int32_t offset) {
        masm.storePtr(r, Address(StackPointer, localOffsetToSPOffset(offset)));
    }

    void storeToFrameF64(FloatRegister r, int32_t offset) {
        masm.storeDouble(r, Address(StackPointer, localOffsetToSPOffset(offset)));
    }

    void storeToFrameF32(FloatRegister r, int32_t offset) {
        masm.storeFloat32(r, Address(StackPointer, localOffsetToSPOffset(offset)));
    }

    void loadFromFrameI32(Register r, int32_t offset) {
        masm.load32(Address(StackPointer, localOffsetToSPOffset(offset)), r);
    }

    void loadFromFrameI64(Register64 r, int32_t offset) {
        masm.load64(Address(StackPointer, localOffsetToSPOffset(offset)), r);
    }

    void loadFromFramePtr(Register r, int32_t offset) {
        masm.loadPtr(Address(StackPointer, localOffsetToSPOffset(offset)), r);
    }

    void loadFromFrameF64(FloatRegister r, int32_t offset) {
        masm.loadDouble(Address(StackPointer, localOffsetToSPOffset(offset)), r);
    }

    void loadFromFrameF32(FloatRegister r, int32_t offset) {
        masm.loadFloat32(Address(StackPointer, localOffsetToSPOffset(offset)), r);
    }

    // Stack-allocated local slots.

    int32_t pushLocal(size_t nbytes) {
        if (nbytes == 8)
            localSize_ = AlignBytes(localSize_, 8u);
        else if (nbytes == 16)
            localSize_ = AlignBytes(localSize_, 16u);
        localSize_ += nbytes;
        return localSize_;          // Locals grow down so capture base address
    }

    int32_t frameOffsetFromSlot(uint32_t slot, MIRType type) {
        MOZ_ASSERT(localInfo_[slot].type() == type);
        return localInfo_[slot].offs();
    }

    ////////////////////////////////////////////////////////////
    //
    // Low-level register allocation.

    bool isAvailable(Register r) {
        return availGPR_.has(r);
    }

    bool hasGPR() {
        return !availGPR_.empty();
    }

    void allocGPR(Register r) {
        MOZ_ASSERT(isAvailable(r));
        availGPR_.take(r);
    }

    Register allocGPR() {
        MOZ_ASSERT(hasGPR());
        return availGPR_.takeAny();
    }

    void freeGPR(Register r) {
        availGPR_.add(r);
    }

    bool isAvailable(Register64 r) {
#ifdef JS_PUNBOX64
        return isAvailable(r.reg);
#else
        return isAvailable(r.low) && isAvailable(r.high);
#endif
    }

    bool hasInt64() {
#ifdef JS_PUNBOX64
        return !availGPR_.empty();
#else
        if (availGPR_.empty())
            return false;
        Register r = allocGPR();
        bool available = !availGPR_.empty();
        freeGPR(r);
        return available;
#endif
    }

    void allocInt64(Register64 r) {
        MOZ_ASSERT(isAvailable(r));
#ifdef JS_PUNBOX64
        availGPR_.take(r.reg);
#else
        availGPR_.take(r.low);
        availGPR_.take(r.high);
#endif
    }

    Register64 allocInt64() {
        MOZ_ASSERT(hasInt64());
#ifdef JS_PUNBOX64
        return Register64(availGPR_.takeAny());
#else
        Register high = availGPR_.takeAny();
        Register low = availGPR_.takeAny();
        return Register64(high, low);
#endif
    }

    void freeInt64(Register64 r) {
#ifdef JS_PUNBOX64
        availGPR_.add(r.reg);
#else
        availGPR_.add(r.low);
        availGPR_.add(r.high);
#endif
    }

    // Notes on float register allocation.
    //
    // The general rule in SpiderMonkey is that float registers can
    // alias double registers, but there are predicates to handle
    // exceptions to that rule: hasUnaliasedDouble() and
    // hasMultiAlias().  The way aliasing actually works is platform
    // dependent and exposed through the aliased(n, &r) predicate,
    // etc.
    //
    //  - hasUnaliasedDouble(): on ARM VFPv3-D32 there are double
    //    registers that cannot be treated as float.
    //  - hasMultiAlias(): on ARM and MIPS a double register aliases
    //    two float registers.
    //  - notes in Architecture-arm.h indicate that when we use a
    //    float register that aliases a double register we only use
    //    the low float register, never the high float register.  I
    //    think those notes lie, or at least are confusing.
    //  - notes in Architecture-mips32.h suggest that the MIPS port
    //    will use both low and high float registers except on the
    //    Longsoon, which may be the only MIPS that's being tested, so
    //    who knows what's working.
    //  - SIMD is not yet implemented on ARM or MIPS so constraints
    //    may change there.
    //
    // On some platforms (x86, x64, ARM64) but not all (ARM)
    // ScratchFloat32Register is the same as ScratchDoubleRegister.
    //
    // It's a basic invariant of the AllocatableRegisterSet that it
    // deals properly with aliasing of registers: if s0 or s1 are
    // allocated then d0 is not allocatable; if s0 and s1 are freed
    // individually then d0 becomes allocatable.

    template<MIRType t>
    FloatRegisters::SetType maskFromTypeFPU() {
        static_assert(t == MIRType::Float32 || t == MIRType::Double, "Float mask type");
        if (t == MIRType::Float32)
            return FloatRegisters::AllSingleMask;
        return FloatRegisters::AllDoubleMask;
    }

    template<MIRType t>
    bool hasFPU() {
        return !!(availFPU_.bits() & maskFromTypeFPU<t>());
    }

    bool isAvailable(FloatRegister r) {
        return availFPU_.has(r);
    }

    void allocFPU(FloatRegister r) {
        MOZ_ASSERT(isAvailable(r));
        availFPU_.take(r);
    }

    template<MIRType t>
    FloatRegister allocFPU() {
        MOZ_ASSERT(hasFPU<t>());
        FloatRegister r =
            FloatRegisterSet::Intersect(FloatRegisterSet(availFPU_.bits()),
                                        FloatRegisterSet(maskFromTypeFPU<t>())).getAny();
        availFPU_.take(r);
        return r;
    }

    void freeFPU(FloatRegister r) {
        availFPU_.add(r);
    }

    ////////////////////////////////////////////////////////////
    //
    // Value stack and high-level register allocation.
    //
    // The value stack facilitates some on-the-fly register allocation
    // and immediate-constant use.  It tracks constants, latent
    // references to locals, register contents, and values on the CPU
    // stack.
    //
    // The stack can be flushed to memory using sync().  This is handy
    // to avoid problems with control flow and messy register usage
    // patterns.

    struct Stk
    {
        enum Kind
        {
            // The Mem opcodes are all clustered at the beginning to
            // allow for a quick test within sync().
            MemI32,               // 32-bit integer stack value ("offs")
            MemI64,               // 64-bit integer stack value ("offs")
            MemF32,               // 32-bit floating stack value ("offs")
            MemF64,               // 64-bit floating stack value ("offs")

            // The Local opcodes follow the Mem opcodes for a similar
            // quick test within hasLocal().
            LocalI32,             // Local int32 var ("slot")
            LocalI64,             // Local int64 var ("slot")
            LocalF32,             // Local float32 var ("slot")
            LocalF64,             // Local double var ("slot")

            RegisterI32,          // 32-bit integer register ("i32reg")
            RegisterI64,          // 64-bit integer register ("i64reg")
            RegisterF32,          // 32-bit floating register ("f32reg")
            RegisterF64,          // 64-bit floating register ("f64reg")

            ConstI32,             // 32-bit integer constant ("i32val")
            ConstI64,             // 64-bit integer constant ("i64val")
            ConstF32,             // 32-bit floating constant ("f32val")
            ConstF64,             // 64-bit floating constant ("f64val")

            None                  // Uninitialized or void
        };

        Kind kind_;

        static const Kind MemLast = MemF64;
        static const Kind LocalLast = LocalF64;

        union {
            RegI32   i32reg_;
            RegI64   i64reg_;
            RegF32   f32reg_;
            RegF64   f64reg_;
            int32_t  i32val_;
            int64_t  i64val_;
            RawF32   f32val_;
            RawF64   f64val_;
            uint32_t slot_;
            uint32_t offs_;
        };

        Stk() { kind_ = None; }

        Kind kind() const { return kind_; }

        RegI32   i32reg() const { MOZ_ASSERT(kind_ == RegisterI32); return i32reg_; }
        RegI64   i64reg() const { MOZ_ASSERT(kind_ == RegisterI64); return i64reg_; }
        RegF32   f32reg() const { MOZ_ASSERT(kind_ == RegisterF32); return f32reg_; }
        RegF64   f64reg() const { MOZ_ASSERT(kind_ == RegisterF64); return f64reg_; }
        int32_t  i32val() const { MOZ_ASSERT(kind_ == ConstI32); return i32val_; }
        int64_t  i64val() const { MOZ_ASSERT(kind_ == ConstI64); return i64val_; }
        RawF32   f32val() const { MOZ_ASSERT(kind_ == ConstF32); return f32val_; }
        RawF64   f64val() const { MOZ_ASSERT(kind_ == ConstF64); return f64val_; }
        uint32_t slot() const { MOZ_ASSERT(kind_ > MemLast && kind_ <= LocalLast); return slot_; }
        uint32_t offs() const { MOZ_ASSERT(kind_ <= MemLast); return offs_; }

        void setI32Reg(RegI32 r) { kind_ = RegisterI32; i32reg_ = r; }
        void setI64Reg(RegI64 r) { kind_ = RegisterI64; i64reg_ = r; }
        void setF32Reg(RegF32 r) { kind_ = RegisterF32; f32reg_ = r; }
        void setF64Reg(RegF64 r) { kind_ = RegisterF64; f64reg_ = r; }
        void setI32Val(int32_t v) { kind_ = ConstI32; i32val_ = v; }
        void setI64Val(int64_t v) { kind_ = ConstI64; i64val_ = v; }
        void setF32Val(RawF32 v) { kind_ = ConstF32; f32val_ = v; }
        void setF64Val(RawF64 v) { kind_ = ConstF64; f64val_ = v; }
        void setSlot(Kind k, uint32_t v) { MOZ_ASSERT(k > MemLast && k <= LocalLast); kind_ = k; slot_ = v; }
        void setOffs(Kind k, uint32_t v) { MOZ_ASSERT(k <= MemLast); kind_ = k; offs_ = v; }
    };

    Vector<Stk, 8, SystemAllocPolicy> stk_;

    Stk& push() {
        stk_.infallibleEmplaceBack(Stk());
        return stk_.back();
    }

    Register64 invalidRegister64() {
        return Register64::Invalid();
    }

    RegI32 invalidI32() {
        return RegI32(Register::Invalid());
    }

    RegI64 invalidI64() {
        return RegI64(invalidRegister64());
    }

    RegF64 invalidF64() {
        return RegF64(InvalidFloatReg);
    }

    RegI32 fromI64(RegI64 r) {
#ifdef JS_PUNBOX64
        return RegI32(r.reg.reg);
#else
        return RegI32(r.reg.low);
#endif
    }

    RegI64 fromI32(RegI32 r) {
        MOZ_ASSERT(!isAvailable(r.reg));
#ifdef JS_PUNBOX64
        return RegI64(Register64(r.reg));
#else
        RegI32 high = needI32();
        return RegI64(Register64(high.reg, r.reg));
#endif
    }

    void freeI32(RegI32 r) {
        freeGPR(r.reg);
    }

    void freeI64(RegI64 r) {
        freeInt64(r.reg);
    }

    void freeI64Except(RegI64 r, RegI32 except) {
#ifdef JS_PUNBOX64
        MOZ_ASSERT(r.reg.reg == except.reg);
#else
        MOZ_ASSERT(r.reg.high == except.reg || r.reg.low == except.reg);
        freeI64(r);
        needI32(except);
#endif


    }

    void freeF64(RegF64 r) {
        freeFPU(r.reg);
    }

    void freeF32(RegF32 r) {
        freeFPU(r.reg);
    }

    MOZ_MUST_USE RegI32 needI32() {
        if (!hasGPR())
            sync();            // TODO / OPTIMIZE: improve this
        return RegI32(allocGPR());
    }

    void needI32(RegI32 specific) {
        if (!isAvailable(specific.reg))
            sync();            // TODO / OPTIMIZE: improve this
        allocGPR(specific.reg);
    }

    // TODO / OPTIMIZE: need2xI32() can be optimized along with needI32()
    // to avoid sync().

    void need2xI32(RegI32 r0, RegI32 r1) {
        needI32(r0);
        needI32(r1);
    }

    MOZ_MUST_USE RegI64 needI64() {
        if (!hasInt64())
            sync();            // TODO / OPTIMIZE: improve this
        return RegI64(allocInt64());
    }

    void needI64(RegI64 specific) {
        if (!isAvailable(specific.reg))
            sync();            // TODO / OPTIMIZE: improve this
        allocInt64(specific.reg);
    }

    void need2xI64(RegI64 r0, RegI64 r1) {
        needI64(r0);
        needI64(r1);
    }

    MOZ_MUST_USE RegF32 needF32() {
        if (!hasFPU<MIRType::Float32>())
            sync();            // TODO / OPTIMIZE: improve this
        return RegF32(allocFPU<MIRType::Float32>());
    }

    void needF32(RegF32 specific) {
        if (!isAvailable(specific.reg))
            sync();            // TODO / OPTIMIZE: improve this
        allocFPU(specific.reg);
    }

    MOZ_MUST_USE RegF64 needF64() {
        if (!hasFPU<MIRType::Double>())
            sync();            // TODO / OPTIMIZE: improve this
        return RegF64(allocFPU<MIRType::Double>());
    }

    void needF64(RegF64 specific) {
        if (!isAvailable(specific.reg))
            sync();            // TODO / OPTIMIZE: improve this
        allocFPU(specific.reg);
    }

    void moveI32(RegI32 src, RegI32 dest) {
        if (src != dest)
            masm.move32(src.reg, dest.reg);
    }

    void moveI64(RegI64 src, RegI64 dest) {
        if (src != dest)
            masm.move64(src.reg, dest.reg);
    }

    void moveF64(RegF64 src, RegF64 dest) {
        if (src != dest)
            masm.moveDouble(src.reg, dest.reg);
    }

    void moveF32(RegF32 src, RegF32 dest) {
        if (src != dest)
            masm.moveFloat32(src.reg, dest.reg);
    }

    void setI64(int64_t v, RegI64 r) {
        masm.move64(Imm64(v), r.reg);
    }

    void loadConstI32(Register r, Stk& src) {
        masm.mov(ImmWord((uint32_t)src.i32val() & 0xFFFFFFFFU), r);
    }

    void loadMemI32(Register r, Stk& src) {
        loadFromFrameI32(r, src.offs());
    }

    void loadLocalI32(Register r, Stk& src) {
        loadFromFrameI32(r, frameOffsetFromSlot(src.slot(), MIRType::Int32));
    }

    void loadRegisterI32(Register r, Stk& src) {
        if (src.i32reg().reg != r)
            masm.move32(src.i32reg().reg, r);
    }

    void loadI32(Register r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstI32:
            loadConstI32(r, src);
            break;
          case Stk::MemI32:
            loadMemI32(r, src);
            break;
          case Stk::LocalI32:
            loadLocalI32(r, src);
            break;
          case Stk::RegisterI32:
            loadRegisterI32(r, src);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: Expected int on stack");
        }
    }

    // TODO / OPTIMIZE: Refactor loadI64, loadF64, and loadF32 in the
    // same way as loadI32 to avoid redundant dispatch in callers of
    // these load() functions.

    void loadI64(Register64 r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstI64:
            masm.move64(Imm64(src.i64val()), r);
            break;
          case Stk::MemI64:
            loadFromFrameI64(r, src.offs());
            break;
          case Stk::LocalI64:
            loadFromFrameI64(r, frameOffsetFromSlot(src.slot(), MIRType::Int64));
            break;
          case Stk::RegisterI64:
            if (src.i64reg().reg != r)
                masm.move64(src.i64reg().reg, r);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: Expected int on stack");
        }
    }

#ifdef JS_NUNBOX32
    void loadI64Low(Register r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstI64:
            masm.move32(Imm64(src.i64val()).low(), r);
            break;
          case Stk::MemI64:
            loadFromFrameI32(r, src.offs() - INT64LOW_OFFSET);
            break;
          case Stk::LocalI64:
            loadFromFrameI32(r, frameOffsetFromSlot(src.slot(), MIRType::Int64) - INT64LOW_OFFSET);
            break;
          case Stk::RegisterI64:
            if (src.i64reg().reg.low != r)
                masm.move32(src.i64reg().reg.low, r);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: Expected int on stack");
        }
    }

    void loadI64High(Register r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstI64:
            masm.move32(Imm64(src.i64val()).hi(), r);
            break;
          case Stk::MemI64:
            loadFromFrameI32(r, src.offs() - INT64HIGH_OFFSET);
            break;
          case Stk::LocalI64:
            loadFromFrameI32(r, frameOffsetFromSlot(src.slot(), MIRType::Int64) - INT64HIGH_OFFSET);
            break;
          case Stk::RegisterI64:
            if (src.i64reg().reg.high != r)
                masm.move32(src.i64reg().reg.high, r);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: Expected int on stack");
        }
    }
#endif

    void loadF64(FloatRegister r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstF64:
            masm.loadConstantDouble(src.f64val(), r);
            break;
          case Stk::MemF64:
            loadFromFrameF64(r, src.offs());
            break;
          case Stk::LocalF64:
            loadFromFrameF64(r, frameOffsetFromSlot(src.slot(), MIRType::Double));
            break;
          case Stk::RegisterF64:
            if (src.f64reg().reg != r)
                masm.moveDouble(src.f64reg().reg, r);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: expected double on stack");
        }
    }

    void loadF32(FloatRegister r, Stk& src) {
        switch (src.kind()) {
          case Stk::ConstF32:
            masm.loadConstantFloat32(src.f32val(), r);
            break;
          case Stk::MemF32:
            loadFromFrameF32(r, src.offs());
            break;
          case Stk::LocalF32:
            loadFromFrameF32(r, frameOffsetFromSlot(src.slot(), MIRType::Float32));
            break;
          case Stk::RegisterF32:
            if (src.f32reg().reg != r)
                masm.moveFloat32(src.f32reg().reg, r);
            break;
          case Stk::None:
            break;
          default:
            MOZ_CRASH("Compiler bug: expected float on stack");
        }
    }

    // Flush all local and register value stack elements to memory.
    //
    // TODO / OPTIMIZE: As this is fairly expensive and causes worse
    // code to be emitted subsequently, it is useful to avoid calling
    // it.
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

    void sync() {
        size_t start = 0;
        size_t lim = stk_.length();

        for (size_t i = lim; i > 0; i--) {
            // Memory opcodes are first in the enum, single check against MemLast is fine.
            if (stk_[i-1].kind() <= Stk::MemLast) {
                start = i;
                break;
            }
        }

        for (size_t i = start; i < lim; i++) {
            Stk& v = stk_[i];
            switch (v.kind()) {
              case Stk::LocalI32: {
                ScratchI32 scratch(*this);
                loadLocalI32(scratch, v);
                masm.Push(scratch);
                v.setOffs(Stk::MemI32, masm.framePushed());
                break;
              }
              case Stk::RegisterI32: {
                masm.Push(v.i32reg().reg);
                freeI32(v.i32reg());
                v.setOffs(Stk::MemI32, masm.framePushed());
                break;
              }
              case Stk::LocalI64: {
                ScratchI32 scratch(*this);
#ifdef JS_PUNBOX64
                loadI64(Register64(scratch), v);
                masm.Push(scratch);
#else
                int32_t offset = frameOffsetFromSlot(v.slot(), MIRType::Int64);
                loadFromFrameI32(scratch, offset - INT64HIGH_OFFSET);
                masm.Push(scratch);
                loadFromFrameI32(scratch, offset - INT64LOW_OFFSET);
                masm.Push(scratch);
#endif
                v.setOffs(Stk::MemI64, masm.framePushed());
                break;
              }
              case Stk::RegisterI64: {
#ifdef JS_PUNBOX64
                masm.Push(v.i64reg().reg.reg);
                freeI64(v.i64reg());
#else
                masm.Push(v.i64reg().reg.high);
                masm.Push(v.i64reg().reg.low);
                freeI64(v.i64reg());
#endif
                v.setOffs(Stk::MemI64, masm.framePushed());
                break;
              }
              case Stk::LocalF64: {
                ScratchF64 scratch(*this);
                loadF64(scratch, v);
                masm.Push(scratch);
                v.setOffs(Stk::MemF64, masm.framePushed());
                break;
              }
              case Stk::RegisterF64: {
                masm.Push(v.f64reg().reg);
                freeF64(v.f64reg());
                v.setOffs(Stk::MemF64, masm.framePushed());
                break;
              }
              case Stk::LocalF32: {
                ScratchF32 scratch(*this);
                loadF32(scratch, v);
                masm.Push(scratch);
                v.setOffs(Stk::MemF32, masm.framePushed());
                break;
              }
              case Stk::RegisterF32: {
                masm.Push(v.f32reg().reg);
                freeF32(v.f32reg());
                v.setOffs(Stk::MemF32, masm.framePushed());
                break;
              }
              default: {
                break;
              }
            }
        }

        maxFramePushed_ = Max(maxFramePushed_, int32_t(masm.framePushed()));
    }

    // This is an optimization used to avoid calling sync() for
    // setLocal(): if the local does not exist unresolved on the stack
    // then we can skip the sync.

    bool hasLocal(uint32_t slot) {
        for (size_t i = stk_.length(); i > 0; i--) {
            // Memory opcodes are first in the enum, single check against MemLast is fine.
            Stk::Kind kind = stk_[i-1].kind();
            if (kind <= Stk::MemLast)
                return false;

            // Local opcodes follow memory opcodes in the enum, single check against
            // LocalLast is sufficient.
            if (kind <= Stk::LocalLast && stk_[i-1].slot() == slot)
                return true;
        }
        return false;
    }

    void syncLocal(uint32_t slot) {
        if (hasLocal(slot))
            sync();            // TODO / OPTIMIZE: Improve this?
    }

    // Push the register r onto the stack.

    void pushI32(RegI32 r) {
        MOZ_ASSERT(!isAvailable(r.reg));
        Stk& x = push();
        x.setI32Reg(r);
    }

    void pushI64(RegI64 r) {
        MOZ_ASSERT(!isAvailable(r.reg));
        Stk& x = push();
        x.setI64Reg(r);
    }

    void pushF64(RegF64 r) {
        MOZ_ASSERT(!isAvailable(r.reg));
        Stk& x = push();
        x.setF64Reg(r);
    }

    void pushF32(RegF32 r) {
        MOZ_ASSERT(!isAvailable(r.reg));
        Stk& x = push();
        x.setF32Reg(r);
    }

    // Push the value onto the stack.

    void pushI32(int32_t v) {
        Stk& x = push();
        x.setI32Val(v);
    }

    void pushI64(int64_t v) {
        Stk& x = push();
        x.setI64Val(v);
    }

    void pushF64(RawF64 v) {
        Stk& x = push();
        x.setF64Val(v);
    }

    void pushF32(RawF32 v) {
        Stk& x = push();
        x.setF32Val(v);
    }

    // Push the local slot onto the stack.  The slot will not be read
    // here; it will be read when it is consumed, or when a side
    // effect to the slot forces its value to be saved.

    void pushLocalI32(uint32_t slot) {
        Stk& x = push();
        x.setSlot(Stk::LocalI32, slot);
    }

    void pushLocalI64(uint32_t slot) {
        Stk& x = push();
        x.setSlot(Stk::LocalI64, slot);
    }

    void pushLocalF64(uint32_t slot) {
        Stk& x = push();
        x.setSlot(Stk::LocalF64, slot);
    }

    void pushLocalF32(uint32_t slot) {
        Stk& x = push();
        x.setSlot(Stk::LocalF32, slot);
    }

    // PRIVATE.  Call only from other popI32() variants.
    // v must be the stack top.

    void popI32(Stk& v, RegI32 r) {
        switch (v.kind()) {
          case Stk::ConstI32:
            loadConstI32(r.reg, v);
            break;
          case Stk::LocalI32:
            loadLocalI32(r.reg, v);
            break;
          case Stk::MemI32:
            masm.Pop(r.reg);
            break;
          case Stk::RegisterI32:
            moveI32(v.i32reg(), r);
            break;
          case Stk::None:
            // This case crops up in situations where there's unreachable code that
            // the type system interprets as "generating" a value of the correct type:
            //
            //   (if (return) E1 E2)                    type is type(E1) meet type(E2)
            //   (if E (unreachable) (i32.const 1))     type is int
            //   (if E (i32.const 1) (unreachable))     type is int
            //
            // It becomes silly to handle this throughout the code, so just handle it
            // here even if that means weaker run-time checking.
            break;
          default:
            MOZ_CRASH("Compiler bug: expected int on stack");
        }
    }

    MOZ_MUST_USE RegI32 popI32() {
        Stk& v = stk_.back();
        RegI32 r;
        if (v.kind() == Stk::RegisterI32)
            r = v.i32reg();
        else
            popI32(v, (r = needI32()));
        stk_.popBack();
        return r;
    }

    RegI32 popI32(RegI32 specific) {
        Stk& v = stk_.back();

        if (!(v.kind() == Stk::RegisterI32 && v.i32reg() == specific)) {
            needI32(specific);
            popI32(v, specific);
            if (v.kind() == Stk::RegisterI32)
                freeI32(v.i32reg());
        }

        stk_.popBack();
        return specific;
    }

    // PRIVATE.  Call only from other popI64() variants.
    // v must be the stack top.

    void popI64(Stk& v, RegI64 r) {
        // TODO / OPTIMIZE: avoid loadI64() here
        switch (v.kind()) {
          case Stk::ConstI64:
          case Stk::LocalI64:
            loadI64(r.reg, v);
            break;
          case Stk::MemI64:
#ifdef JS_PUNBOX64
            masm.Pop(r.reg.reg);
#else
            masm.Pop(r.reg.low);
            masm.Pop(r.reg.high);
#endif
            break;
          case Stk::RegisterI64:
            moveI64(v.i64reg(), r);
            break;
          case Stk::None:
            // See popI32()
            break;
          default:
            MOZ_CRASH("Compiler bug: expected long on stack");
        }
    }

    MOZ_MUST_USE RegI64 popI64() {
        Stk& v = stk_.back();
        RegI64 r;
        if (v.kind() == Stk::RegisterI64)
            r = v.i64reg();
        else
            popI64(v, (r = needI64()));
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
            if (v.kind() == Stk::RegisterI64)
                freeI64(v.i64reg());
        }

        stk_.popBack();
        return specific;
    }

    // PRIVATE.  Call only from other popF64() variants.
    // v must be the stack top.

    void popF64(Stk& v, RegF64 r) {
        // TODO / OPTIMIZE: avoid loadF64 here
        switch (v.kind()) {
          case Stk::ConstF64:
          case Stk::LocalF64:
            loadF64(r.reg, v);
            break;
          case Stk::MemF64:
            masm.Pop(r.reg);
            break;
          case Stk::RegisterF64:
            moveF64(v.f64reg(), r);
            break;
          case Stk::None:
            // See popI32()
            break;
          default:
            MOZ_CRASH("Compiler bug: expected double on stack");
        }
    }

    MOZ_MUST_USE RegF64 popF64() {
        Stk& v = stk_.back();
        RegF64 r;
        if (v.kind() == Stk::RegisterF64)
            r = v.f64reg();
        else
            popF64(v, (r = needF64()));
        stk_.popBack();
        return r;
    }

    RegF64 popF64(RegF64 specific) {
        Stk& v = stk_.back();

        if (!(v.kind() == Stk::RegisterF64 && v.f64reg() == specific)) {
            needF64(specific);
            popF64(v, specific);
            if (v.kind() == Stk::RegisterF64)
                freeF64(v.f64reg());
        }

        stk_.popBack();
        return specific;
    }

    // PRIVATE.  Call only from other popF32() variants.
    // v must be the stack top.

    void popF32(Stk& v, RegF32 r) {
        // TODO / OPTIMIZE: avoid loadF32 here
        switch (v.kind()) {
          case Stk::ConstF32:
          case Stk::LocalF32:
            loadF32(r.reg, v);
            break;
          case Stk::MemF32:
            masm.Pop(r.reg);
            break;
          case Stk::RegisterF32:
            moveF32(v.f32reg(), r);
            break;
          case Stk::None:
            // See popI32()
            break;
          default:
            MOZ_CRASH("Compiler bug: expected float on stack");
        }
    }

    MOZ_MUST_USE RegF32 popF32() {
        Stk& v = stk_.back();
        RegF32 r;
        if (v.kind() == Stk::RegisterF32)
            r = v.f32reg();
        else
            popF32(v, (r = needF32()));
        stk_.popBack();
        return r;
    }

    RegF32 popF32(RegF32 specific) {
        Stk& v = stk_.back();

        if (!(v.kind() == Stk::RegisterF32 && v.f32reg() == specific)) {
            needF32(specific);
            popF32(v, specific);
            if (v.kind() == Stk::RegisterF32)
                freeF32(v.f32reg());
        }

        stk_.popBack();
        return specific;
    }

    MOZ_MUST_USE bool popConstI32(int32_t& c) {
        Stk& v = stk_.back();
        if (v.kind() != Stk::ConstI32)
            return false;
        c = v.i32val();
        stk_.popBack();
        return true;
    }

    // TODO / OPTIMIZE: At the moment we use ReturnReg for JoinReg.
    // It is possible other choices would lead to better register
    // allocation, as ReturnReg is often first in the register set and
    // will be heavily wanted by the register allocator that uses
    // takeFirst().
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

    MOZ_MUST_USE AnyReg popJoinReg() {
        switch (stk_.back().kind()) {
          case Stk::RegisterI32:
          case Stk::ConstI32:
          case Stk::MemI32:
          case Stk::LocalI32:
            return AnyReg(popI32(joinRegI32));
          case Stk::RegisterI64:
          case Stk::ConstI64:
          case Stk::MemI64:
          case Stk::LocalI64:
            return AnyReg(popI64(joinRegI64));
          case Stk::RegisterF64:
          case Stk::ConstF64:
          case Stk::MemF64:
          case Stk::LocalF64:
            return AnyReg(popF64(joinRegF64));
          case Stk::RegisterF32:
          case Stk::ConstF32:
          case Stk::MemF32:
          case Stk::LocalF32:
            return AnyReg(popF32(joinRegF32));
          case Stk::None:
            stk_.popBack();
            return AnyReg();
          default:
            MOZ_CRASH("Compiler bug: unexpected value on stack");
        }
    }

    MOZ_MUST_USE AnyReg allocJoinReg(ExprType type) {
        switch (type) {
          case ExprType::I32:
            allocGPR(joinRegI32.reg);
            return AnyReg(joinRegI32);
          case ExprType::I64:
            allocInt64(joinRegI64.reg);
            return AnyReg(joinRegI64);
          case ExprType::F32:
            allocFPU(joinRegF32.reg);
            return AnyReg(joinRegF32);
          case ExprType::F64:
            allocFPU(joinRegF64.reg);
            return AnyReg(joinRegF64);
          case ExprType::Void:
            MOZ_CRASH("Compiler bug: allocating void join reg");
          default:
            MOZ_CRASH("Compiler bug: unexpected type");
        }
    }

    void pushJoinReg(AnyReg r) {
        switch (r.tag) {
          case AnyReg::NONE:
            MOZ_CRASH("Compile bug: attempting to push void");
            break;
          case AnyReg::I32:
            pushI32(r.i32());
            break;
          case AnyReg::I64:
            pushI64(r.i64());
            break;
          case AnyReg::F64:
            pushF64(r.f64());
            break;
          case AnyReg::F32:
            pushF32(r.f32());
            break;
        }
    }

    void freeJoinReg(AnyReg r) {
        switch (r.tag) {
          case AnyReg::NONE:
            MOZ_CRASH("Compile bug: attempting to free void reg");
            break;
          case AnyReg::I32:
            freeI32(r.i32());
            break;
          case AnyReg::I64:
            freeI64(r.i64());
            break;
          case AnyReg::F64:
            freeF64(r.f64());
            break;
          case AnyReg::F32:
            freeF32(r.f32());
            break;
        }
    }

    void maybeReserveJoinRegI(ExprType type) {
        if (type == ExprType::I32)
            needI32(joinRegI32);
        else if (type == ExprType::I64)
            needI64(joinRegI64);
    }

    void maybeUnreserveJoinRegI(ExprType type) {
        if (type == ExprType::I32)
            freeI32(joinRegI32);
        else if (type == ExprType::I64)
            freeI64(joinRegI64);
    }

    // Return the amount of execution stack consumed by the top numval
    // values on the value stack.

    size_t stackConsumed(size_t numval) {
        size_t size = 0;
        MOZ_ASSERT(numval <= stk_.length());
        for (uint32_t i = stk_.length()-1; numval > 0; numval--, i--) {
            // The size computations come from the implementation of Push() in
            // MacroAssembler-x86-shared.cpp and MacroAssembler-arm-shared.cpp,
            // and from VFPRegister::size() in Architecture-arm.h.
            //
            // On ARM unlike on x86 we push a single for float.

            Stk& v = stk_[i];
            switch (v.kind()) {
              case Stk::MemI32:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
                size += sizeof(intptr_t);
#else
                MOZ_CRASH("BaseCompiler platform hook: stackConsumed I32");
#endif
                break;
              case Stk::MemI64:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
                size += sizeof(int64_t);
#else
                MOZ_CRASH("BaseCompiler platform hook: stackConsumed I64");
#endif
                break;
              case Stk::MemF64:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
                size += sizeof(double);
#else
                MOZ_CRASH("BaseCompiler platform hook: stackConsumed F64");
#endif
                break;
              case Stk::MemF32:
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
                size += sizeof(double);
#elif defined(JS_CODEGEN_ARM)
                size += sizeof(float);
#else
                MOZ_CRASH("BaseCompiler platform hook: stackConsumed F32");
#endif
                break;
              default:
                break;
            }
        }
        return size;
    }

    void popValueStackTo(uint32_t stackSize) {
        for (uint32_t i = stk_.length(); i > stackSize; i--) {
            Stk& v = stk_[i-1];
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
              default:
                break;
            }
        }
        stk_.shrinkTo(stackSize);
    }

    void popValueStackBy(uint32_t items) {
        popValueStackTo(stk_.length() - items);
    }

    // Before branching to an outer control label, pop the execution
    // stack to the level expected by that region, but do not free the
    // stack as that will happen as compilation leaves the block.

    void popStackBeforeBranch(uint32_t framePushed) {
        uint32_t frameHere = masm.framePushed();
        if (frameHere > framePushed)
            masm.addPtr(ImmWord(frameHere - framePushed), StackPointer);
    }

    // Before exiting a nested control region, pop the execution stack
    // to the level expected by the nesting region, and free the
    // stack.

    void popStackOnBlockExit(uint32_t framePushed) {
        uint32_t frameHere = masm.framePushed();
        if (frameHere > framePushed) {
            if (deadCode_)
                masm.adjustStack(frameHere - framePushed);
            else
                masm.freeStack(frameHere - framePushed);
        }
    }

    // Peek at the stack, for calls.

    Stk& peek(uint32_t relativeDepth) {
        return stk_[stk_.length()-1-relativeDepth];
    }

    ////////////////////////////////////////////////////////////
    //
    // Control stack

    Vector<Control, 8, SystemAllocPolicy> ctl_;

    MOZ_MUST_USE bool pushControl(UniquePooledLabel* label, UniquePooledLabel* otherLabel = nullptr)
    {
        uint32_t framePushed = masm.framePushed();
        uint32_t stackSize = stk_.length();

        if (!ctl_.emplaceBack(Control(framePushed, stackSize)))
            return false;
        if (label)
            ctl_.back().label = label->release();
        if (otherLabel)
            ctl_.back().otherLabel = otherLabel->release();
        ctl_.back().deadOnArrival = deadCode_;
        return true;
    }

    void popControl() {
        Control last = ctl_.popCopy();
        if (last.label)
            freeLabel(last.label);
        if (last.otherLabel)
            freeLabel(last.otherLabel);

        if (deadCode_ && !ctl_.empty())
            popValueStackTo(ctl_.back().stackSize);
    }

    Control& controlItem(uint32_t relativeDepth) {
        return ctl_[ctl_.length() - 1 - relativeDepth];
    }

    MOZ_MUST_USE PooledLabel* newLabel() {
        // TODO / INVESTIGATE: allocate() is fallible, but we can
        // probably rely on an infallible allocator here.  That would
        // simplify code later.
        PooledLabel* candidate = labelPool_.allocate();
        if (!candidate)
            return nullptr;
        return new (candidate) PooledLabel(this);
    }

    void freeLabel(PooledLabel* label) {
        label->~PooledLabel();
        labelPool_.free(label);
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Function prologue and epilogue.

    void beginFunction() {
        JitSpew(JitSpew_Codegen, "# Emitting wasm baseline code");

        SigIdDesc sigId = mg_.funcDefSigs[func_.defIndex()]->id;
        GenerateFunctionPrologue(masm, localSize_, sigId, prologueTrapOffset_,
                                 &compileResults_.offsets());

        MOZ_ASSERT(masm.framePushed() == uint32_t(localSize_));

        maxFramePushed_ = localSize_;

        // We won't know until after we've generated code how big the
        // frame will be (we may need arbitrary spill slots and
        // outgoing param slots) so branch to code emitted after the
        // function body that will perform the check.
        //
        // Code there will also assume that the fixed-size stack frame
        // has been allocated.

        masm.jump(&outOfLinePrologue_);
        masm.bind(&bodyLabel_);

        // Copy arguments from registers to stack.

        const ValTypeVector& args = func_.sig().args();

        for (ABIArgIter<const ValTypeVector> i(args); !i.done(); i++) {
            Local& l = localInfo_[i.index()];
            switch (i.mirType()) {
              case MIRType::Int32:
                if (i->argInRegister())
                    storeToFrameI32(i->gpr(), l.offs());
                break;
              case MIRType::Int64:
                if (i->argInRegister())
                    storeToFrameI64(i->gpr64(), l.offs());
                break;
              case MIRType::Double:
                if (i->argInRegister())
                    storeToFrameF64(i->fpu(), l.offs());
                break;
              case MIRType::Float32:
                if (i->argInRegister())
                    storeToFrameF32(i->fpu(), l.offs());
                break;
              default:
                MOZ_CRASH("Function argument type");
            }
        }

        // The TLS pointer is always passed as a hidden argument in WasmTlsReg.
        // Save it into its assigned local slot.
        storeToFramePtr(WasmTlsReg, localInfo_[tlsSlot_].offs());

        // Initialize the stack locals to zero.
        //
        // TODO / OPTIMIZE: on x64, at least, scratch will be a 64-bit
        // register and we can move 64 bits at a time.
        //
        // TODO / OPTIMIZE: On SSE2 or better SIMD systems we may be
        // able to store 128 bits at a time.  (I suppose on some
        // systems we have 512-bit SIMD for that matter.)
        //
        // TODO / OPTIMIZE: if we have only one initializing store
        // then it's better to store a zero literal, probably.

        if (varLow_ < varHigh_) {
            ScratchI32 scratch(*this);
            masm.mov(ImmWord(0), scratch);
            for (int32_t i = varLow_ ; i < varHigh_ ; i+=4)
                storeToFrameI32(scratch, i+4);
        }
    }

    bool endFunction() {
        // Out-of-line prologue.  Assumes that the in-line prologue has
        // been executed and that a frame of size = localSize_ + sizeof(AsmJSFrame)
        // has been allocated.

        masm.bind(&outOfLinePrologue_);

        MOZ_ASSERT(maxFramePushed_ >= localSize_);

        // ABINonArgReg0 != ScratchReg, which can be used by branchPtr().

        masm.movePtr(masm.getStackPointer(), ABINonArgReg0);
        masm.subPtr(Imm32(maxFramePushed_ - localSize_), ABINonArgReg0);
        masm.branchPtr(Assembler::Below,
                       Address(WasmTlsReg, offsetof(TlsData, stackLimit)),
                       ABINonArgReg0,
                       &bodyLabel_);

        // Since we just overflowed the stack, to be on the safe side, pop the
        // stack so that, when the trap exit stub executes, it is a safe
        // distance away from the end of the native stack.
        if (localSize_)
            masm.addToStackPtr(Imm32(localSize_));
        masm.jump(TrapDesc(prologueTrapOffset_, Trap::StackOverflow, /* framePushed = */ 0));

        masm.bind(&returnLabel_);

        // Restore the TLS register in case it was overwritten by the function.
        loadFromFramePtr(WasmTlsReg, frameOffsetFromSlot(tlsSlot_, MIRType::Pointer));

        GenerateFunctionEpilogue(masm, localSize_, &compileResults_.offsets());

#if defined(JS_ION_PERF)
        // FIXME - profiling code missing.  Bug 1286948.

        // Note the end of the inline code and start of the OOL code.
        //gen->perfSpewer().noteEndInlineCode(masm);
#endif

        if (!generateOutOfLineCode())
            return false;

        masm.wasmEmitTrapOutOfLineCode();

        compileResults_.offsets().end = masm.currentOffset();

        // A frame greater than 256KB is implausible, probably an attack,
        // so fail the compilation.

        if (maxFramePushed_ > 256 * 1024)
            return false;

        return true;
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Calls.

    struct FunctionCall
    {
        explicit FunctionCall(uint32_t lineOrBytecode)
          : lineOrBytecode(lineOrBytecode),
            reloadMachineStateAfter(false),
            usesSystemAbi(false),
            loadTlsBefore(false),
#ifdef JS_CODEGEN_ARM
            hardFP(true),
#endif
            frameAlignAdjustment(0),
            stackArgAreaSize(0)
        {}

        uint32_t lineOrBytecode;
        ABIArgGenerator abi;
        bool reloadMachineStateAfter;
        bool usesSystemAbi;
        bool loadTlsBefore;
#ifdef JS_CODEGEN_ARM
        bool hardFP;
#endif
        size_t frameAlignAdjustment;
        size_t stackArgAreaSize;
    };

    void beginCall(FunctionCall& call, UseABI useABI, InterModule interModule)
    {
        call.reloadMachineStateAfter = interModule == InterModule::True || useABI == UseABI::System;
        call.usesSystemAbi = useABI == UseABI::System;
        call.loadTlsBefore = useABI == UseABI::Wasm;

        if (call.usesSystemAbi) {
            // Call-outs need to use the appropriate system ABI.
#if defined(JS_CODEGEN_ARM)
# if defined(JS_SIMULATOR_ARM)
            call.hardFP = UseHardFpABI();
# elif defined(JS_CODEGEN_ARM_HARDFP)
            call.hardFP = true;
# else
            call.hardFP = false;
# endif
            call.abi.setUseHardFp(call.hardFP);
#endif
        }

        call.frameAlignAdjustment = ComputeByteAlignment(masm.framePushed() + sizeof(AsmJSFrame),
                                                         JitStackAlignment);
    }

    void endCall(FunctionCall& call)
    {
        size_t adjustment = call.stackArgAreaSize + call.frameAlignAdjustment;
        if (adjustment)
            masm.freeStack(adjustment);

        if (call.reloadMachineStateAfter) {
            loadFromFramePtr(WasmTlsReg, frameOffsetFromSlot(tlsSlot_, MIRType::Pointer));
            masm.loadWasmPinnedRegsFromTls();
        }
    }

    // TODO / OPTIMIZE: This is expensive; let's roll the iterator
    // walking into the walking done for passArg.  See comments in
    // passArg.

    size_t stackArgAreaSize(const ValTypeVector& args) {
        ABIArgIter<const ValTypeVector> i(args);
        while (!i.done())
            i++;
        return AlignBytes(i.stackBytesConsumedSoFar(), 16u);
    }

    void startCallArgs(FunctionCall& call, size_t stackArgAreaSize)
    {
        call.stackArgAreaSize = stackArgAreaSize;

        size_t adjustment = call.stackArgAreaSize + call.frameAlignAdjustment;
        if (adjustment)
            masm.reserveStack(adjustment);
    }

    const ABIArg reservePointerArgument(FunctionCall& call) {
        return call.abi.next(MIRType::Pointer);
    }

    // TODO / OPTIMIZE: Note passArg is used only in one place.  (Or it was,
    // until Luke wandered through, but that can be fixed again.)  I'm not
    // saying we should manually inline it, but we could hoist the dispatch into
    // the caller and have type-specific implementations of passArg:
    // passArgI32(), etc.  Then those might be inlined, at least in PGO builds.
    //
    // The bulk of the work here (60%) is in the next() call, though.
    //
    // Notably, since next() is so expensive, stackArgAreaSize() becomes
    // expensive too.
    //
    // Somehow there could be a trick here where the sequence of
    // argument types (read from the input stream) leads to a cached
    // entry for stackArgAreaSize() and for how to pass arguments...
    //
    // But at least we could reduce the cost of stackArgAreaSize() by
    // first reading the argument types into a (reusable) vector, then
    // we have the outgoing size at low cost, and then we can pass
    // args based on the info we read.

    void passArg(FunctionCall& call, ValType type, Stk& arg) {
        switch (type) {
          case ValType::I32: {
            ABIArg argLoc = call.abi.next(MIRType::Int32);
            if (argLoc.kind() == ABIArg::Stack) {
                ScratchI32 scratch(*this);
                loadI32(scratch, arg);
                masm.store32(scratch, Address(StackPointer, argLoc.offsetFromArgBase()));
            } else {
                loadI32(argLoc.gpr(), arg);
            }
            break;
          }
          case ValType::I64: {
            ABIArg argLoc = call.abi.next(MIRType::Int64);
            if (argLoc.kind() == ABIArg::Stack) {
                ScratchI32 scratch(*this);
#if defined(JS_CODEGEN_X64)
                loadI64(Register64(scratch), arg);
                masm.movq(scratch, Operand(StackPointer, argLoc.offsetFromArgBase()));
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
                loadI64Low(scratch, arg);
                masm.store32(scratch, Address(StackPointer, argLoc.offsetFromArgBase() + INT64LOW_OFFSET));
                loadI64High(scratch, arg);
                masm.store32(scratch, Address(StackPointer, argLoc.offsetFromArgBase() + INT64HIGH_OFFSET));
#else
                MOZ_CRASH("BaseCompiler platform hook: passArg I64");
#endif
            } else {
                loadI64(argLoc.gpr64(), arg);
            }
            break;
          }
          case ValType::F64: {
            ABIArg argLoc = call.abi.next(MIRType::Double);
            switch (argLoc.kind()) {
              case ABIArg::Stack: {
                ScratchF64 scratch(*this);
                loadF64(scratch, arg);
                masm.storeDouble(scratch, Address(StackPointer, argLoc.offsetFromArgBase()));
                break;
              }
#if defined(JS_CODEGEN_REGISTER_PAIR)
              case ABIArg::GPR_PAIR: {
# ifdef JS_CODEGEN_ARM
                ScratchF64 scratch(*this);
                loadF64(scratch, arg);
                masm.ma_vxfer(scratch, argLoc.evenGpr(), argLoc.oddGpr());
                break;
# else
                MOZ_CRASH("BaseCompiler platform hook: passArg F64 pair");
# endif
              }
#endif
              case ABIArg::FPU: {
                loadF64(argLoc.fpu(), arg);
                break;
              }
              case ABIArg::GPR: {
                MOZ_CRASH("Unexpected parameter passing discipline");
              }
            }
            break;
          }
          case ValType::F32: {
            ABIArg argLoc = call.abi.next(MIRType::Float32);
            switch (argLoc.kind()) {
              case ABIArg::Stack: {
                ScratchF32 scratch(*this);
                loadF32(scratch, arg);
                masm.storeFloat32(scratch, Address(StackPointer, argLoc.offsetFromArgBase()));
                break;
              }
              case ABIArg::GPR: {
                ScratchF32 scratch(*this);
                loadF32(scratch, arg);
                masm.moveFloat32ToGPR(scratch, argLoc.gpr());
                break;
              }
              case ABIArg::FPU: {
                loadF32(argLoc.fpu(), arg);
                break;
              }
#if defined(JS_CODEGEN_REGISTER_PAIR)
              case ABIArg::GPR_PAIR: {
                MOZ_CRASH("Unexpected parameter passing discipline");
              }
#endif
            }
            break;
          }
          default:
            MOZ_CRASH("Function argument type");
        }
    }

    void callDefinition(uint32_t funcDefIndex, const FunctionCall& call)
    {
        CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::FuncDef);
        masm.call(desc, funcDefIndex);
    }

    void callSymbolic(SymbolicAddress callee, const FunctionCall& call) {
        CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
        masm.call(callee);
    }

    // Precondition: sync()

    void callIndirect(uint32_t sigIndex, Stk& indexVal, const FunctionCall& call)
    {
        loadI32(WasmTableCallIndexReg, indexVal);

        const SigWithId& sig = mg_.sigs[sigIndex];

        CalleeDesc callee;
        if (isCompilingAsmJS()) {
            MOZ_ASSERT(sig.id.kind() == SigIdDesc::Kind::None);
            const TableDesc& table = mg_.tables[mg_.asmJSSigToTableIndex[sigIndex]];

            MOZ_ASSERT(IsPowerOfTwo(table.limits.initial));
            masm.andPtr(Imm32((table.limits.initial - 1)), WasmTableCallIndexReg);

            callee = CalleeDesc::asmJSTable(table);
        } else {
            MOZ_ASSERT(sig.id.kind() != SigIdDesc::Kind::None);
            MOZ_ASSERT(mg_.tables.length() == 1);
            const TableDesc& table = mg_.tables[0];

            callee = CalleeDesc::wasmTable(table, sig.id);
        }

        CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Dynamic);
        masm.wasmCallIndirect(desc, callee);
    }

    // Precondition: sync()

    void callImport(unsigned globalDataOffset, const FunctionCall& call)
    {
        CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Dynamic);
        CalleeDesc callee = CalleeDesc::import(globalDataOffset);
        masm.wasmCallImport(desc, callee);
    }

    void builtinCall(SymbolicAddress builtin, const FunctionCall& call)
    {
        callSymbolic(builtin, call);
    }

    void builtinInstanceMethodCall(SymbolicAddress builtin, const ABIArg& instanceArg,
                                   const FunctionCall& call)
    {
        // Builtin method calls assume the TLS register has been set.
        loadFromFramePtr(WasmTlsReg, frameOffsetFromSlot(tlsSlot_, MIRType::Pointer));

        CallSiteDesc desc(call.lineOrBytecode, CallSiteDesc::Symbolic);
        masm.wasmCallBuiltinInstanceMethod(instanceArg, builtin);
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Sundry low-level code generators.

    void addInterruptCheck()
    {
        // Always use signals for interrupts with Asm.JS/Wasm
        MOZ_RELEASE_ASSERT(HaveSignalHandlers());
    }

    void jumpTable(LabelVector& labels) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        for (uint32_t i = 0; i < labels.length(); i++) {
            CodeLabel cl;
            masm.writeCodePointer(cl.patchAt());
            cl.target()->bind(labels[i]->offset());
            masm.addCodeLabel(cl);
        }
#else
        MOZ_CRASH("BaseCompiler platform hook: jumpTable");
#endif
    }

    void tableSwitch(Label* theTable, RegI32 switchValue) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        ScratchI32 scratch(*this);
        CodeLabel tableCl;

        masm.mov(tableCl.patchAt(), scratch);

        tableCl.target()->bind(theTable->offset());
        masm.addCodeLabel(tableCl);

        masm.jmp(Operand(scratch, switchValue.reg, ScalePointer));
#elif defined(JS_CODEGEN_ARM)
        ScratchI32 scratch(*this);

        // Compute the offset from the next instruction to the jump table
        Label here;
        masm.bind(&here);
        uint32_t offset = here.offset() - theTable->offset();

        // Read PC+8
        masm.ma_mov(pc, scratch);

        // Required by ma_sub.
        ScratchRegisterScope arm_scratch(*this);

        // Compute the table base pointer
        masm.ma_sub(Imm32(offset + 8), scratch, arm_scratch);

        // Jump indirect via table element
        masm.ma_ldr(DTRAddr(scratch, DtrRegImmShift(switchValue.reg, LSL, 2)), pc, Offset,
                    Assembler::Always);
#else
        MOZ_CRASH("BaseCompiler platform hook: tableSwitch");
#endif
    }

    RegI32 captureReturnedI32() {
        RegI32 rv = RegI32(ReturnReg);
        MOZ_ASSERT(isAvailable(rv.reg));
        needI32(rv);
        return rv;
    }

    RegI64 captureReturnedI64() {
        RegI64 rv = RegI64(ReturnReg64);
        MOZ_ASSERT(isAvailable(rv.reg));
        needI64(rv);
        return rv;
    }

    RegF32 captureReturnedF32(const FunctionCall& call) {
        RegF32 rv = RegF32(ReturnFloat32Reg);
        MOZ_ASSERT(isAvailable(rv.reg));
        needF32(rv);
#if defined(JS_CODEGEN_X86)
        if (call.usesSystemAbi) {
            masm.reserveStack(sizeof(float));
            Operand op(esp, 0);
            masm.fstp32(op);
            masm.loadFloat32(op, rv.reg);
            masm.freeStack(sizeof(float));
        }
#elif defined(JS_CODEGEN_ARM)
        if (call.usesSystemAbi && !call.hardFP)
            masm.ma_vxfer(r0, rv.reg);
#endif
        return rv;
    }

    RegF64 captureReturnedF64(const FunctionCall& call) {
        RegF64 rv = RegF64(ReturnDoubleReg);
        MOZ_ASSERT(isAvailable(rv.reg));
        needF64(rv);
#if defined(JS_CODEGEN_X86)
        if (call.usesSystemAbi) {
            masm.reserveStack(sizeof(double));
            Operand op(esp, 0);
            masm.fstp(op);
            masm.loadDouble(op, rv.reg);
            masm.freeStack(sizeof(double));
        }
#elif defined(JS_CODEGEN_ARM)
        if (call.usesSystemAbi && !call.hardFP)
            masm.ma_vxfer(r0, r1, rv.reg);
#endif
        return rv;
    }

    void returnCleanup() {
        popStackBeforeBranch(ctl_[0].framePushed);
        masm.jump(&returnLabel_);
    }

    void subtractI64(RegI64 rhs, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.sub64(rhs.reg, srcDest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: subtractI64");
#endif
    }

    void multiplyI64(RegI64 rhs, RegI64 srcDest, RegI32 temp) {
#if defined(JS_CODEGEN_X64)
        MOZ_ASSERT(srcDest.reg.reg == rax);
        MOZ_ASSERT(isAvailable(rdx));
        masm.mul64(rhs.reg, srcDest.reg, temp.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.mul64(rhs.reg, srcDest.reg, temp.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: multiplyI64");
#endif
    }

    void checkDivideByZeroI32(RegI32 rhs, RegI32 srcDest, Label* done) {
        if (isCompilingAsmJS()) {
            // Truncated division by zero is zero (Infinity|0 == 0)
            Label notDivByZero;
            masm.branchTest32(Assembler::NonZero, rhs.reg, rhs.reg, &notDivByZero);
            masm.move32(Imm32(0), srcDest.reg);
            masm.jump(done);
            masm.bind(&notDivByZero);
        } else {
            masm.branchTest32(Assembler::Zero, rhs.reg, rhs.reg, trap(Trap::IntegerDivideByZero));
        }
    }

    void checkDivideByZeroI64(RegI64 rhs, RegI64 srcDest, Label* done) {
        MOZ_ASSERT(!isCompilingAsmJS());
#if defined(JS_CODEGEN_X64)
        masm.testq(rhs.reg.reg, rhs.reg.reg);
        masm.j(Assembler::Zero, trap(Trap::IntegerDivideByZero));
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        Label nonZero;
        masm.branchTest32(Assembler::NonZero, rhs.reg.low, rhs.reg.low, &nonZero);
        masm.branchTest32(Assembler::Zero, rhs.reg.high, rhs.reg.high, trap(Trap::IntegerDivideByZero));
        masm.bind(&nonZero);
#else
        MOZ_CRASH("BaseCompiler platform hook: checkDivideByZeroI64");
#endif
    }

    void checkDivideSignedOverflowI32(RegI32 rhs, RegI32 srcDest, Label* done, bool zeroOnOverflow) {
        Label notMin;
        masm.branch32(Assembler::NotEqual, srcDest.reg, Imm32(INT32_MIN), &notMin);
        if (zeroOnOverflow) {
            masm.branch32(Assembler::NotEqual, rhs.reg, Imm32(-1), &notMin);
            masm.move32(Imm32(0), srcDest.reg);
            masm.jump(done);
        } else if (isCompilingAsmJS()) {
            // (-INT32_MIN)|0 == INT32_MIN and INT32_MIN is already in srcDest.
            masm.branch32(Assembler::Equal, rhs.reg, Imm32(-1), done);
        } else {
            masm.branch32(Assembler::Equal, rhs.reg, Imm32(-1), trap(Trap::IntegerOverflow));
        }
        masm.bind(&notMin);
    }

    void checkDivideSignedOverflowI64(RegI64 rhs, RegI64 srcDest, Label* done, bool zeroOnOverflow) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        MOZ_ASSERT(!isCompilingAsmJS());
        Label notmin;
        masm.branch64(Assembler::NotEqual, srcDest.reg, Imm64(INT64_MIN), &notmin);
        masm.branch64(Assembler::NotEqual, rhs.reg, Imm64(-1), &notmin);
        if (zeroOnOverflow) {
            masm.xor64(srcDest.reg, srcDest.reg);
            masm.jump(done);
        } else {
            masm.jump(trap(Trap::IntegerOverflow));
        }
        masm.bind(&notmin);
#else
        MOZ_CRASH("BaseCompiler platform hook: checkDivideSignedOverflowI64");
#endif
    }

#ifndef QUOT_REM_I64_CALLOUT
    void quotientI64(RegI64 rhs, RegI64 srcDest, IsUnsigned isUnsigned) {
        Label done;

        checkDivideByZeroI64(rhs, srcDest, &done);

        if (!isUnsigned)
            checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(false));

# if defined(JS_CODEGEN_X64)
        // The caller must set up the following situation.
        MOZ_ASSERT(srcDest.reg.reg == rax);
        MOZ_ASSERT(isAvailable(rdx));
        if (isUnsigned) {
            masm.xorq(rdx, rdx);
            masm.udivq(rhs.reg.reg);
        } else {
            masm.cqo();
            masm.idivq(rhs.reg.reg);
        }
# else
        MOZ_CRASH("BaseCompiler platform hook: quotientI64");
# endif
        masm.bind(&done);
    }

    void remainderI64(RegI64 rhs, RegI64 srcDest, IsUnsigned isUnsigned) {
        Label done;

        checkDivideByZeroI64(rhs, srcDest, &done);

        if (!isUnsigned)
            checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(true));

# if defined(JS_CODEGEN_X64)
        // The caller must set up the following situation.
        MOZ_ASSERT(srcDest.reg.reg == rax);
        MOZ_ASSERT(isAvailable(rdx));

        if (isUnsigned) {
            masm.xorq(rdx, rdx);
            masm.udivq(rhs.reg.reg);
        } else {
            masm.cqo();
            masm.idivq(rhs.reg.reg);
        }
        masm.movq(rdx, rax);
# else
        MOZ_CRASH("BaseCompiler platform hook: remainderI64");
# endif
        masm.bind(&done);
    }
#endif

    void orI64(RegI64 rhs, RegI64 srcDest) {
        masm.or64(rhs.reg, srcDest.reg);
    }

    void andI64(RegI64 rhs, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.and64(rhs.reg, srcDest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: andI64");
#endif
    }

    void xorI64(RegI64 rhs, RegI64 srcDest) {
        masm.xor64(rhs.reg, srcDest.reg);
    }

    // Note, may destroy RHS too.
    void lshiftI32(RegI32 rhs, RegI32 srcDest) {
#if defined(JS_CODEGEN_ARM)
        masm.and32(Imm32(31), rhs.reg);
#endif
        masm.lshift32(rhs.reg, srcDest.reg);
    }

    void lshiftI64(RegI64 shift, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.lshift64(shift.reg.reg, srcDest.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.lshift64(shift.reg.low, srcDest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: lshiftI64");
#endif
    }

    // Note, may destroy RHS too.
    void rshiftI32(RegI32 rhs, RegI32 srcDest) {
#if defined(JS_CODEGEN_ARM)
        masm.and32(Imm32(31), rhs.reg);
#endif
        masm.rshift32Arithmetic(rhs.reg, srcDest.reg);
    }

    // Note, may destroy RHS too.
    void rshiftIU32(RegI32 rhs, RegI32 srcDest) {
#if defined(JS_CODEGEN_ARM)
        masm.and32(Imm32(31), rhs.reg);
#endif
        masm.rshift32(rhs.reg, srcDest.reg);
    }

    void rshiftI64(RegI64 shift, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.rshift64Arithmetic(shift.reg.reg, srcDest.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.rshift64Arithmetic(shift.reg.low, srcDest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: rshiftI64");
#endif
    }

    void rshiftU64(RegI64 shift, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.rshift64(shift.reg.reg, srcDest.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.rshift64(shift.reg.low, srcDest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: rshiftU64");
#endif
    }

    void rotateRightI64(RegI64 shift, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.rotateRight64(shift.reg.reg, srcDest.reg, srcDest.reg, Register::Invalid());
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.rotateRight64(shift.reg.low, srcDest.reg, srcDest.reg, shift.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: rotateRightI64");
#endif
    }

    void rotateLeftI64(RegI64 shift, RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.rotateLeft64(shift.reg.reg, srcDest.reg, srcDest.reg, Register::Invalid());
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.rotateLeft64(shift.reg.low, srcDest.reg, srcDest.reg, shift.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: rotateLeftI64");
#endif
    }

    void clzI64(RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.clz64(srcDest.reg, srcDest.reg.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.clz64(srcDest.reg, srcDest.reg.low);
        masm.move32(Imm32(0), srcDest.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: clzI64");
#endif
    }

    void ctzI64(RegI64 srcDest) {
#if defined(JS_CODEGEN_X64)
        masm.ctz64(srcDest.reg, srcDest.reg.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        masm.ctz64(srcDest.reg, srcDest.reg.low);
        masm.move32(Imm32(0), srcDest.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: ctzI64");
#endif
    }

    bool popcnt32NeedsTemp() const {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        return !AssemblerX86Shared::HasPOPCNT();
#elif defined(JS_CODEGEN_ARM)
        return true;
#else
        MOZ_CRASH("BaseCompiler platform hook: popcnt32NeedsTemp");
#endif
    }

    void popcntI32(RegI32 srcDest, RegI32 tmp) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM)
        masm.popcnt32(srcDest.reg, srcDest.reg, tmp.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: popcntI32");
#endif
    }

    bool popcnt64NeedsTemp() const {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        return !AssemblerX86Shared::HasPOPCNT();
#elif defined(JS_CODEGEN_ARM)
        return true;
#else
        MOZ_CRASH("BaseCompiler platform hook: popcnt64NeedsTemp");
#endif
    }

    void popcntI64(RegI64 srcDest, RegI32 tmp) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM)
        masm.popcnt64(srcDest.reg, srcDest.reg, tmp.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: popcntI64");
#endif
    }

    void reinterpretI64AsF64(RegI64 src, RegF64 dest) {
#if defined(JS_CODEGEN_X64)
        masm.vmovq(src.reg.reg, dest.reg);
#elif defined(JS_CODEGEN_X86)
        masm.Push(src.reg.high);
        masm.Push(src.reg.low);
        masm.vmovq(Operand(esp, 0), dest.reg);
        masm.freeStack(sizeof(uint64_t));
#elif defined(JS_CODEGEN_ARM)
        masm.ma_vxfer(src.reg.low, src.reg.high, dest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: reinterpretI64AsF64");
#endif
    }

    void reinterpretF64AsI64(RegF64 src, RegI64 dest) {
#if defined(JS_CODEGEN_X64)
        masm.vmovq(src.reg, dest.reg.reg);
#elif defined(JS_CODEGEN_X86)
        masm.reserveStack(sizeof(uint64_t));
        masm.vmovq(src.reg, Operand(esp, 0));
        masm.Pop(dest.reg.low);
        masm.Pop(dest.reg.high);
#elif defined(JS_CODEGEN_ARM)
        masm.ma_vxfer(src.reg, dest.reg.low, dest.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: reinterpretF64AsI64");
#endif
    }

    void wrapI64ToI32(RegI64 src, RegI32 dest) {
#if defined(JS_CODEGEN_X64)
        // movl clears the high bits if the two registers are the same.
        masm.movl(src.reg.reg, dest.reg);
#elif defined(JS_NUNBOX32)
        masm.move32(src.reg.low, dest.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: wrapI64ToI32");
#endif
    }

    void extendI32ToI64(RegI32 src, RegI64 dest) {
#if defined(JS_CODEGEN_X64)
        masm.movslq(src.reg, dest.reg.reg);
#elif defined(JS_CODEGEN_X86)
        MOZ_ASSERT(dest.reg.low == src.reg);
        MOZ_ASSERT(dest.reg.low == eax);
        MOZ_ASSERT(dest.reg.high == edx);
        masm.cdq();
#elif defined(JS_CODEGEN_ARM)
        masm.ma_mov(src.reg, dest.reg.low);
        masm.ma_asr(Imm32(31), src.reg, dest.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: extendI32ToI64");
#endif
    }

    void extendU32ToI64(RegI32 src, RegI64 dest) {
#if defined(JS_CODEGEN_X64)
        masm.movl(src.reg, dest.reg.reg);
#elif defined(JS_NUNBOX32)
        masm.move32(src.reg, dest.reg.low);
        masm.move32(Imm32(0), dest.reg.high);
#else
        MOZ_CRASH("BaseCompiler platform hook: extendU32ToI64");
#endif
    }

    class OutOfLineTruncateF32OrF64ToI32 : public OutOfLineCode
    {
        AnyReg src;
        RegI32 dest;
        bool isAsmJS;
        bool isUnsigned;
        TrapOffset off;

      public:
        OutOfLineTruncateF32OrF64ToI32(AnyReg src, RegI32 dest, bool isAsmJS, bool isUnsigned,
                                       TrapOffset off)
          : src(src),
            dest(dest),
            isAsmJS(isAsmJS),
            isUnsigned(isUnsigned),
            off(off)
        {
            MOZ_ASSERT_IF(isAsmJS, !isUnsigned);
        }

        virtual void generate(MacroAssembler& masm) {
            bool isFloat = src.tag == AnyReg::F32;
            FloatRegister fsrc = isFloat ? src.f32().reg : src.f64().reg;
            if (isAsmJS) {
                saveVolatileReturnGPR(masm);
                masm.outOfLineTruncateSlow(fsrc, dest.reg, isFloat, /* isAsmJS */ true);
                restoreVolatileReturnGPR(masm);
                masm.jump(rejoin());
            } else {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
                if (isFloat)
                    masm.outOfLineWasmTruncateFloat32ToInt32(fsrc, isUnsigned, off, rejoin());
                else
                    masm.outOfLineWasmTruncateDoubleToInt32(fsrc, isUnsigned, off, rejoin());
#elif defined(JS_CODEGEN_ARM)
                masm.outOfLineWasmTruncateToIntCheck(fsrc,
                                                     isFloat ? MIRType::Float32 : MIRType::Double,
                                                     MIRType::Int32, isUnsigned, rejoin(), off);
#else
                (void)isUnsigned; // Suppress warnings
                (void)off;        //  for unused private
                MOZ_CRASH("BaseCompiler platform hook: OutOfLineTruncateF32OrF64ToI32 wasm");
#endif
            }
        }
    };

    MOZ_MUST_USE bool truncateF32ToI32(RegF32 src, RegI32 dest, bool isUnsigned) {
        TrapOffset off = trapOffset();
        OutOfLineCode* ool;
        if (isCompilingAsmJS()) {
            ool = new(alloc_) OutOfLineTruncateF32OrF64ToI32(AnyReg(src), dest, true, false, off);
            ool = addOutOfLineCode(ool);
            if (!ool)
                return false;
            masm.branchTruncateFloat32ToInt32(src.reg, dest.reg, ool->entry());
        } else {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM)
            ool = new(alloc_) OutOfLineTruncateF32OrF64ToI32(AnyReg(src), dest, false, isUnsigned,
                                                             off);
            ool = addOutOfLineCode(ool);
            if (!ool)
                return false;
            if (isUnsigned)
                masm.wasmTruncateFloat32ToUInt32(src.reg, dest.reg, ool->entry());
            else
                masm.wasmTruncateFloat32ToInt32(src.reg, dest.reg, ool->entry());
#else
            MOZ_CRASH("BaseCompiler platform hook: truncateF32ToI32 wasm");
#endif
        }
        masm.bind(ool->rejoin());
        return true;
    }

    MOZ_MUST_USE bool truncateF64ToI32(RegF64 src, RegI32 dest, bool isUnsigned) {
        TrapOffset off = trapOffset();
        OutOfLineCode* ool;
        if (isCompilingAsmJS()) {
            ool = new(alloc_) OutOfLineTruncateF32OrF64ToI32(AnyReg(src), dest, true, false, off);
            ool = addOutOfLineCode(ool);
            if (!ool)
                return false;
            masm.branchTruncateDoubleToInt32(src.reg, dest.reg, ool->entry());
        } else {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_ARM)
            ool = new(alloc_) OutOfLineTruncateF32OrF64ToI32(AnyReg(src), dest, false, isUnsigned,
                                                             off);
            ool = addOutOfLineCode(ool);
            if (!ool)
                return false;
            if (isUnsigned)
                masm.wasmTruncateDoubleToUInt32(src.reg, dest.reg, ool->entry());
            else
                masm.wasmTruncateDoubleToInt32(src.reg, dest.reg, ool->entry());
#else
            MOZ_CRASH("BaseCompiler platform hook: truncateF64ToI32 wasm");
#endif
        }
        masm.bind(ool->rejoin());
        return true;
    }

    // This does not generate a value; if the truncation failed then it traps.

    class OutOfLineTruncateCheckF32OrF64ToI64 : public OutOfLineCode
    {
        AnyReg src;
        bool isUnsigned;
        TrapOffset off;

      public:
        OutOfLineTruncateCheckF32OrF64ToI64(AnyReg src, bool isUnsigned, TrapOffset off)
          : src(src),
            isUnsigned(isUnsigned),
            off(off)
        {}

        virtual void generate(MacroAssembler& masm) {
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
            if (src.tag == AnyReg::F32)
                masm.outOfLineWasmTruncateFloat32ToInt64(src.f32().reg, isUnsigned, off, rejoin());
            else if (src.tag == AnyReg::F64)
                masm.outOfLineWasmTruncateDoubleToInt64(src.f64().reg, isUnsigned, off, rejoin());
            else
                MOZ_CRASH("unexpected type");
#elif defined(JS_CODEGEN_ARM)
            if (src.tag == AnyReg::F32)
                masm.outOfLineWasmTruncateToIntCheck(src.f32().reg, MIRType::Float32,
                                                     MIRType::Int64, isUnsigned, rejoin(), off);
            else if (src.tag == AnyReg::F64)
                masm.outOfLineWasmTruncateToIntCheck(src.f64().reg, MIRType::Double, MIRType::Int64,
                                                     isUnsigned, rejoin(), off);
            else
                MOZ_CRASH("unexpected type");
#else
            MOZ_CRASH("BaseCompiler platform hook: OutOfLineTruncateCheckF32OrF64ToI64");
#endif
        }
    };

#ifndef FLOAT_TO_I64_CALLOUT
    MOZ_MUST_USE bool truncateF32ToI64(RegF32 src, RegI64 dest, bool isUnsigned, RegF64 temp) {
# if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        OutOfLineCode* ool =
            addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(AnyReg(src),
                                                                              isUnsigned,
                                                                              trapOffset()));
        if (!ool)
            return false;
        if (isUnsigned)
            masm.wasmTruncateFloat32ToUInt64(src.reg, dest.reg, ool->entry(),
                                             ool->rejoin(), temp.reg);
        else
            masm.wasmTruncateFloat32ToInt64(src.reg, dest.reg, ool->entry(),
                                            ool->rejoin(), temp.reg);
# else
        MOZ_CRASH("BaseCompiler platform hook: truncateF32ToI64");
# endif
        return true;
    }

    MOZ_MUST_USE bool truncateF64ToI64(RegF64 src, RegI64 dest, bool isUnsigned, RegF64 temp) {
# if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        OutOfLineCode* ool =
            addOutOfLineCode(new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(AnyReg(src),
                                                                              isUnsigned,
                                                                              trapOffset()));
        if (!ool)
            return false;
        if (isUnsigned)
            masm.wasmTruncateDoubleToUInt64(src.reg, dest.reg, ool->entry(),
                                            ool->rejoin(), temp.reg);
        else
            masm.wasmTruncateDoubleToInt64(src.reg, dest.reg, ool->entry(),
                                           ool->rejoin(), temp.reg);
# else
        MOZ_CRASH("BaseCompiler platform hook: truncateF64ToI64");
# endif
        return true;
    }
#endif

#ifndef I64_TO_FLOAT_CALLOUT
    bool convertI64ToFloatNeedsTemp(bool isUnsigned) const {
# if defined(JS_CODEGEN_X86)
        return isUnsigned && AssemblerX86Shared::HasSSE3();
# else
        return false;
# endif
    }

    void convertI64ToF32(RegI64 src, bool isUnsigned, RegF32 dest, RegI32 temp) {
# if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        if (isUnsigned)
            masm.convertUInt64ToFloat32(src.reg, dest.reg, temp.reg);
        else
            masm.convertInt64ToFloat32(src.reg, dest.reg);
# else
        MOZ_CRASH("BaseCompiler platform hook: convertI64ToF32");
# endif
    }

    void convertI64ToF64(RegI64 src, bool isUnsigned, RegF64 dest, RegI32 temp) {
# if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
        if (isUnsigned)
            masm.convertUInt64ToDouble(src.reg, dest.reg, temp.reg);
        else
            masm.convertInt64ToDouble(src.reg, dest.reg);
# else
        MOZ_CRASH("BaseCompiler platform hook: convertI64ToF64");
# endif
    }
#endif

    void cmp64Set(Assembler::Condition cond, RegI64 lhs, RegI64 rhs, RegI32 dest) {
#if defined(JS_CODEGEN_X64)
        masm.cmpq(rhs.reg.reg, lhs.reg.reg);
        masm.emitSet(cond, dest.reg);
#elif defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
        // TODO / OPTIMIZE: This is pretty branchy, we should be able to do better.
        Label done, condTrue;
        masm.branch64(cond, lhs.reg, rhs.reg, &condTrue);
        masm.move32(Imm32(0), dest.reg);
        masm.jump(&done);
        masm.bind(&condTrue);
        masm.move32(Imm32(1), dest.reg);
        masm.bind(&done);
#else
        MOZ_CRASH("BaseCompiler platform hook: cmp64Set");
#endif
    }

    void unreachableTrap()
    {
        masm.jump(trap(Trap::Unreachable));
#ifdef DEBUG
        masm.breakpoint();
#endif
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Global variable access.

    // CodeGenerator{X86,X64}::visitAsmJSLoadGlobalVar()

    void loadGlobalVarI32(unsigned globalDataOffset, RegI32 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.loadRipRelativeInt32(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.movlWithPatch(PatchedAbsoluteAddress(), r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        ScratchRegisterScope scratch(*this); // Really must be the ARM scratchreg
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_dtr(js::jit::IsLoad, GlobalReg, Imm32(addr), r.reg, scratch);
#else
        MOZ_CRASH("BaseCompiler platform hook: loadGlobalVarI32");
#endif
    }

    void loadGlobalVarI64(unsigned globalDataOffset, RegI64 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.loadRipRelativeInt64(r.reg.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset labelLow = masm.movlWithPatch(PatchedAbsoluteAddress(), r.reg.low);
        masm.append(GlobalAccess(labelLow, globalDataOffset + INT64LOW_OFFSET));
        CodeOffset labelHigh = masm.movlWithPatch(PatchedAbsoluteAddress(), r.reg.high);
        masm.append(GlobalAccess(labelHigh, globalDataOffset + INT64HIGH_OFFSET));
#elif defined(JS_CODEGEN_ARM)
        ScratchRegisterScope scratch(*this); // Really must be the ARM scratchreg
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_dtr(js::jit::IsLoad, GlobalReg, Imm32(addr + INT64LOW_OFFSET), r.reg.low, scratch);
        masm.ma_dtr(js::jit::IsLoad, GlobalReg, Imm32(addr + INT64HIGH_OFFSET), r.reg.high,
                    scratch);
#else
        MOZ_CRASH("BaseCompiler platform hook: loadGlobalVarI64");
#endif
    }

    void loadGlobalVarF32(unsigned globalDataOffset, RegF32 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.loadRipRelativeFloat32(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.vmovssWithPatch(PatchedAbsoluteAddress(), r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        VFPRegister vd(r.reg);
        masm.ma_vldr(VFPAddr(GlobalReg, VFPOffImm(addr)), vd.singleOverlay());
#else
        MOZ_CRASH("BaseCompiler platform hook: loadGlobalVarF32");
#endif
    }

    void loadGlobalVarF64(unsigned globalDataOffset, RegF64 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.loadRipRelativeDouble(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.vmovsdWithPatch(PatchedAbsoluteAddress(), r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_vldr(VFPAddr(GlobalReg, VFPOffImm(addr)), r.reg);
#else
        MOZ_CRASH("BaseCompiler platform hook: loadGlobalVarF64");
#endif
    }

    // CodeGeneratorX64::visitAsmJSStoreGlobalVar()

    void storeGlobalVarI32(unsigned globalDataOffset, RegI32 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.storeRipRelativeInt32(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.movlWithPatch(r.reg, PatchedAbsoluteAddress());
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        ScratchRegisterScope scratch(*this); // Really must be the ARM scratchreg
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_dtr(js::jit::IsStore, GlobalReg, Imm32(addr), r.reg, scratch);
#else
        MOZ_CRASH("BaseCompiler platform hook: storeGlobalVarI32");
#endif
    }

    void storeGlobalVarI64(unsigned globalDataOffset, RegI64 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.storeRipRelativeInt64(r.reg.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset labelLow = masm.movlWithPatch(r.reg.low, PatchedAbsoluteAddress());
        masm.append(GlobalAccess(labelLow, globalDataOffset + INT64LOW_OFFSET));
        CodeOffset labelHigh = masm.movlWithPatch(r.reg.high, PatchedAbsoluteAddress());
        masm.append(GlobalAccess(labelHigh, globalDataOffset + INT64HIGH_OFFSET));
#elif defined(JS_CODEGEN_ARM)
        ScratchRegisterScope scratch(*this); // Really must be the ARM scratchreg
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_dtr(js::jit::IsStore, GlobalReg, Imm32(addr + INT64LOW_OFFSET), r.reg.low, scratch);
        masm.ma_dtr(js::jit::IsStore, GlobalReg, Imm32(addr + INT64HIGH_OFFSET), r.reg.high,
                    scratch);
#else
        MOZ_CRASH("BaseCompiler platform hook: storeGlobalVarI64");
#endif
    }

    void storeGlobalVarF32(unsigned globalDataOffset, RegF32 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.storeRipRelativeFloat32(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.vmovssWithPatch(r.reg, PatchedAbsoluteAddress());
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        VFPRegister vd(r.reg);
        masm.ma_vstr(vd.singleOverlay(), VFPAddr(GlobalReg, VFPOffImm(addr)));
#else
        MOZ_CRASH("BaseCompiler platform hook: storeGlobalVarF32");
#endif
    }

    void storeGlobalVarF64(unsigned globalDataOffset, RegF64 r)
    {
#if defined(JS_CODEGEN_X64)
        CodeOffset label = masm.storeRipRelativeDouble(r.reg);
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_X86)
        CodeOffset label = masm.vmovsdWithPatch(r.reg, PatchedAbsoluteAddress());
        masm.append(GlobalAccess(label, globalDataOffset));
#elif defined(JS_CODEGEN_ARM)
        unsigned addr = globalDataOffset - AsmJSGlobalRegBias;
        masm.ma_vstr(r.reg, VFPAddr(GlobalReg, VFPOffImm(addr)));
#else
        MOZ_CRASH("BaseCompiler platform hook: storeGlobalVarF64");
#endif
    }

    //////////////////////////////////////////////////////////////////////
    //
    // Heap access.

#ifndef WASM_HUGE_MEMORY
    class AsmJSLoadOOB : public OutOfLineCode
    {
        Scalar::Type viewType;
        AnyRegister dest;

      public:
        AsmJSLoadOOB(Scalar::Type viewType, AnyRegister dest)
          : viewType(viewType),
            dest(dest)
        {}

        void generate(MacroAssembler& masm) {
# if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
            switch (viewType) {
              case Scalar::Float32x4:
              case Scalar::Int32x4:
              case Scalar::Int8x16:
              case Scalar::Int16x8:
              case Scalar::MaxTypedArrayViewType:
                MOZ_CRASH("unexpected array type");
              case Scalar::Float32:
                masm.loadConstantFloat32(float(GenericNaN()), dest.fpu());
                break;
              case Scalar::Float64:
                masm.loadConstantDouble(GenericNaN(), dest.fpu());
                break;
              case Scalar::Int8:
              case Scalar::Uint8:
              case Scalar::Int16:
              case Scalar::Uint16:
              case Scalar::Int32:
              case Scalar::Uint32:
              case Scalar::Uint8Clamped:
                masm.movePtr(ImmWord(0), dest.gpr());
                break;
              case Scalar::Int64:
                MOZ_CRASH("unexpected array type");
            }
            masm.jump(rejoin());
# else
            Unused << viewType;
            Unused << dest;
            MOZ_CRASH("Compiler bug: Unexpected platform.");
# endif
        }
    };
#endif

  private:
    void checkOffset(MemoryAccessDesc* access, RegI32 ptr) {
        if (access->offset() >= OffsetGuardLimit) {
            masm.branchAdd32(Assembler::CarrySet,
                             Imm32(access->offset()), ptr.reg,
                             trap(Trap::OutOfBounds));
            access->clearOffset();
        }
    }

  public:
    // This is the temp register passed as the last argument to load()
    MOZ_MUST_USE size_t loadStoreTemps(MemoryAccessDesc& access) {
#if defined(JS_CODEGEN_ARM)
        if (access.isUnaligned()) {
            switch (access.type()) {
              case Scalar::Float32:
                return 1;
              case Scalar::Float64:
                return 2;
              default:
                break;
            }
        }
        return 0;
#else
        return 0;
#endif
    }

    // ptr and dest may be the same iff dest is I32.
    // This may destroy ptr even if ptr and dest are not the same.
    MOZ_MUST_USE bool load(MemoryAccessDesc& access, RegI32 ptr, AnyReg dest, RegI32 tmp1,
                           RegI32 tmp2)
    {
        checkOffset(&access, ptr);

        OutOfLineCode* ool = nullptr;
#ifndef WASM_HUGE_MEMORY
        if (access.isPlainAsmJS()) {
            ool = new (alloc_) AsmJSLoadOOB(access.type(), dest.any());
            if (!addOutOfLineCode(ool))
                return false;

            masm.wasmBoundsCheck(Assembler::AboveOrEqual, ptr.reg, ool->entry());
        } else {
            masm.wasmBoundsCheck(Assembler::AboveOrEqual, ptr.reg, trap(Trap::OutOfBounds));
        }
#endif

#if defined(JS_CODEGEN_X64)
        Operand srcAddr(HeapReg, ptr.reg, TimesOne, access.offset());

        if (dest.tag == AnyReg::I64)
            masm.wasmLoadI64(access, srcAddr, dest.i64().reg);
        else
            masm.wasmLoad(access, srcAddr, dest.any());
#elif defined(JS_CODEGEN_X86)
        Operand srcAddr(ptr.reg, access.offset());

        if (dest.tag == AnyReg::I64) {
            masm.wasmLoadI64(access, srcAddr, dest.i64().reg);
        } else {
            bool byteRegConflict = access.byteSize() == 1 && !singleByteRegs_.has(dest.i32().reg);
            AnyRegister out = byteRegConflict ? AnyRegister(ScratchRegX86) : dest.any();

            masm.wasmLoad(access, srcAddr, out);

            if (byteRegConflict)
                masm.mov(ScratchRegX86, dest.i32().reg);
        }
#elif defined(JS_CODEGEN_ARM)
        if (access.offset() != 0)
            masm.add32(Imm32(access.offset()), ptr.reg);

        bool isSigned = true;
        switch (access.type()) {
          case Scalar::Uint8:
          case Scalar::Uint16:
          case Scalar::Uint32: {
            isSigned = false;
            MOZ_FALLTHROUGH;
          case Scalar::Int8:
          case Scalar::Int16:
          case Scalar::Int32:
            Register rt = dest.tag == AnyReg::I64 ? dest.i64().reg.low : dest.i32().reg;
            loadI32(access, isSigned, ptr, rt);
            if (dest.tag == AnyReg::I64) {
                if (isSigned)
                    masm.ma_asr(Imm32(31), rt, dest.i64().reg.high);
                else
                    masm.move32(Imm32(0), dest.i64().reg.high);
            }
            break;
          }
          case Scalar::Int64:
            loadI64(access, ptr, dest.i64());
            break;
          case Scalar::Float32:
            loadF32(access, ptr, dest.f32(), tmp1);
            break;
          case Scalar::Float64:
            loadF64(access, ptr, dest.f64(), tmp1, tmp2);
            break;
          default:
            MOZ_CRASH("Compiler bug: unexpected array type");
        }
#else
        MOZ_CRASH("BaseCompiler platform hook: load");
#endif

        if (ool)
            masm.bind(ool->rejoin());
        return true;
    }

    // ptr and src must not be the same register.
    // This may destroy ptr.
    MOZ_MUST_USE bool store(MemoryAccessDesc access, RegI32 ptr, AnyReg src, RegI32 tmp1,
                            RegI32 tmp2)
    {
        checkOffset(&access, ptr);

        Label rejoin;
#ifndef WASM_HUGE_MEMORY
        if (access.isPlainAsmJS())
            masm.wasmBoundsCheck(Assembler::AboveOrEqual, ptr.reg, &rejoin);
        else
            masm.wasmBoundsCheck(Assembler::AboveOrEqual, ptr.reg, trap(Trap::OutOfBounds));
#endif

        // Emit the store
#if defined(JS_CODEGEN_X64)
        Operand dstAddr(HeapReg, ptr.reg, TimesOne, access.offset());

        masm.wasmStore(access, src.any(), dstAddr);
#elif defined(JS_CODEGEN_X86)
        Operand dstAddr(ptr.reg, access.offset());

        if (access.type() == Scalar::Int64) {
            masm.wasmStoreI64(access, src.i64().reg, dstAddr);
        } else {
            AnyRegister value;
            if (src.tag == AnyReg::I64) {
                value = AnyRegister(src.i64().reg.low);
            } else if (access.byteSize() == 1 && !singleByteRegs_.has(src.i32().reg)) {
                masm.mov(src.i32().reg, ScratchRegX86);
                value = AnyRegister(ScratchRegX86);
            } else {
                value = src.any();
            }

            masm.wasmStore(access, value, dstAddr);
        }
#elif defined(JS_CODEGEN_ARM)
        if (access.offset() != 0)
            masm.add32(Imm32(access.offset()), ptr.reg);

        switch (access.type()) {
          case Scalar::Uint8:
            MOZ_FALLTHROUGH;
          case Scalar::Uint16:
            MOZ_FALLTHROUGH;
          case Scalar::Int8:
            MOZ_FALLTHROUGH;
          case Scalar::Int16:
            MOZ_FALLTHROUGH;
          case Scalar::Int32:
            MOZ_FALLTHROUGH;
          case Scalar::Uint32: {
            Register rt = src.tag == AnyReg::I64 ? src.i64().reg.low : src.i32().reg;
            storeI32(access, ptr, rt);
            break;
          }
          case Scalar::Int64:
            storeI64(access, ptr, src.i64());
            break;
          case Scalar::Float32:
            storeF32(access, ptr, src.f32(), tmp1);
            break;
          case Scalar::Float64:
            storeF64(access, ptr, src.f64(), tmp1, tmp2);
            break;
          default:
            MOZ_CRASH("Compiler bug: unexpected array type");
        }
#else
        MOZ_CRASH("BaseCompiler platform hook: store");
#endif

        if (rejoin.used())
            masm.bind(&rejoin);

        return true;
    }

#ifdef JS_CODEGEN_ARM
    void
    loadI32(MemoryAccessDesc access, bool isSigned, RegI32 ptr, Register rt) {
        if (access.byteSize() > 1 && access.isUnaligned()) {
            masm.add32(HeapReg, ptr.reg);
            SecondScratchRegisterScope scratch(*this);
            masm.emitUnalignedLoad(isSigned, access.byteSize(), ptr.reg, scratch, rt, 0);
        } else {
            BufferOffset ld =
                masm.ma_dataTransferN(js::jit::IsLoad, BitSize(access.byteSize()*8),
                                      isSigned, HeapReg, ptr.reg, rt, Offset, Assembler::Always);
            masm.append(access, ld.getOffset(), masm.framePushed());
        }
    }

    void
    storeI32(MemoryAccessDesc access, RegI32 ptr, Register rt) {
        if (access.byteSize() > 1 && access.isUnaligned()) {
            masm.add32(HeapReg, ptr.reg);
            masm.emitUnalignedStore(access.byteSize(), ptr.reg, rt, 0);
        } else {
            BufferOffset st =
                masm.ma_dataTransferN(js::jit::IsStore, BitSize(access.byteSize()*8),
                                      IsSigned(false), ptr.reg, HeapReg, rt, Offset,
                                      Assembler::Always);
            masm.append(access, st.getOffset(), masm.framePushed());
        }
    }

    void
    loadI64(MemoryAccessDesc access, RegI32 ptr, RegI64 dest) {
        if (access.isUnaligned()) {
            masm.add32(HeapReg, ptr.reg);
            SecondScratchRegisterScope scratch(*this);
            masm.emitUnalignedLoad(IsSigned(false), ByteSize(4), ptr.reg, scratch, dest.reg.low,
                                   0);
            masm.emitUnalignedLoad(IsSigned(false), ByteSize(4), ptr.reg, scratch, dest.reg.high,
                                   4);
        } else {
            BufferOffset ld;
            ld = masm.ma_dataTransferN(js::jit::IsLoad, BitSize(32), IsSigned(false), HeapReg,
                                       ptr.reg, dest.reg.low, Offset, Assembler::Always);
            masm.append(access, ld.getOffset(), masm.framePushed());
            masm.add32(Imm32(4), ptr.reg);
            ld = masm.ma_dataTransferN(js::jit::IsLoad, BitSize(32), IsSigned(false), HeapReg,
                                       ptr.reg, dest.reg.high, Offset, Assembler::Always);
            masm.append(access, ld.getOffset(), masm.framePushed());
        }
    }

    void
    storeI64(MemoryAccessDesc access, RegI32 ptr, RegI64 src) {
        if (access.isUnaligned()) {
            masm.add32(HeapReg, ptr.reg);
            masm.emitUnalignedStore(ByteSize(4), ptr.reg, src.reg.low, 0);
            masm.emitUnalignedStore(ByteSize(4), ptr.reg, src.reg.high, 4);
        } else {
            BufferOffset st;
            st = masm.ma_dataTransferN(js::jit::IsStore, BitSize(32), IsSigned(false), HeapReg,
                                       ptr.reg, src.reg.low, Offset, Assembler::Always);
            masm.append(access, st.getOffset(), masm.framePushed());
            masm.add32(Imm32(4), ptr.reg);
            st = masm.ma_dataTransferN(js::jit::IsStore, BitSize(32), IsSigned(false), HeapReg,
                                       ptr.reg, src.reg.high, Offset, Assembler::Always);
            masm.append(access, st.getOffset(), masm.framePushed());
        }
    }

    void
    loadF32(MemoryAccessDesc access, RegI32 ptr, RegF32 dest, RegI32 tmp1) {
        masm.add32(HeapReg, ptr.reg);
        if (access.isUnaligned()) {
            SecondScratchRegisterScope scratch(*this);
            masm.emitUnalignedLoad(IsSigned(false), ByteSize(4), ptr.reg, scratch, tmp1.reg, 0);
            masm.ma_vxfer(tmp1.reg, dest.reg);
        } else {
            BufferOffset ld = masm.ma_vldr(VFPAddr(ptr.reg, VFPOffImm(0)), dest.reg,
                                           Assembler::Always);
            masm.append(access, ld.getOffset(), masm.framePushed());
        }
    }

    void
    storeF32(MemoryAccessDesc access, RegI32 ptr, RegF32 src, RegI32 tmp1) {
        masm.add32(HeapReg, ptr.reg);
        if (access.isUnaligned()) {
            masm.ma_vxfer(src.reg, tmp1.reg);
            masm.emitUnalignedStore(ByteSize(4), ptr.reg, tmp1.reg, 0);
        } else {
            BufferOffset st =
                masm.ma_vstr(src.reg, VFPAddr(ptr.reg, VFPOffImm(0)), Assembler::Always);
            masm.append(access, st.getOffset(), masm.framePushed());
        }
    }

    void
    loadF64(MemoryAccessDesc access, RegI32 ptr, RegF64 dest, RegI32 tmp1, RegI32 tmp2) {
        masm.add32(HeapReg, ptr.reg);
        if (access.isUnaligned()) {
            SecondScratchRegisterScope scratch(*this);
            masm.emitUnalignedLoad(IsSigned(false), ByteSize(4), ptr.reg, scratch, tmp1.reg, 0);
            masm.emitUnalignedLoad(IsSigned(false), ByteSize(4), ptr.reg, scratch, tmp2.reg, 4);
            masm.ma_vxfer(tmp1.reg, tmp2.reg, dest.reg);
        } else {
            BufferOffset ld = masm.ma_vldr(VFPAddr(ptr.reg, VFPOffImm(0)), dest.reg,
                                           Assembler::Always);
            masm.append(access, ld.getOffset(), masm.framePushed());
        }
    }

    void
    storeF64(MemoryAccessDesc access, RegI32 ptr, RegF64 src, RegI32 tmp1, RegI32 tmp2) {
        masm.add32(HeapReg, ptr.reg);
        if (access.isUnaligned()) {
            masm.ma_vxfer(src.reg, tmp1.reg, tmp2.reg);
            masm.emitUnalignedStore(ByteSize(4), ptr.reg, tmp1.reg, 0);
            masm.emitUnalignedStore(ByteSize(4), ptr.reg, tmp2.reg, 4);
        } else {
            BufferOffset st =
                masm.ma_vstr(src.reg, VFPAddr(ptr.reg, VFPOffImm(0)), Assembler::Always);
            masm.append(access, st.getOffset(), masm.framePushed());
        }
    }
#endif

    ////////////////////////////////////////////////////////////

    // Generally speaking, ABOVE this point there should be no value
    // stack manipulation (calls to popI32 etc).

    // Generally speaking, BELOW this point there should be no
    // platform dependencies.  We make an exception for x86 register
    // targeting, which is not too hard to keep clean.

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

    void pop2xF32(RegF32* r0, RegF32* r1) {
        *r1 = popF32();
        *r0 = popF32();
    }

    void pop2xF64(RegF64* r0, RegF64* r1) {
        *r1 = popF64();
        *r0 = popF64();
    }

    ////////////////////////////////////////////////////////////
    //
    // Sundry helpers.

    uint32_t readCallSiteLineOrBytecode() {
        if (!func_.callSiteLineNums().empty())
            return func_.callSiteLineNums()[lastReadCallSite_++];
        return trapOffset().bytecodeOffset;
    }

    bool done() const {
        return iter_.done();
    }

    bool isCompilingAsmJS() const {
        return mg_.kind == ModuleKind::AsmJS;
    }

    TrapOffset trapOffset() const {
        return iter_.trapOffset();
    }
    Maybe<TrapOffset> trapIfNotAsmJS() const {
        return isCompilingAsmJS() ? Nothing() : Some(trapOffset());
    }
    TrapDesc trap(Trap t) const {
        return TrapDesc(trapOffset(), t, masm.framePushed());
    }

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
    MOZ_MUST_USE bool emitReturn();
    MOZ_MUST_USE bool emitCallArgs(const ValTypeVector& args, FunctionCall& baselineCall);
    MOZ_MUST_USE bool emitCallImportCommon(uint32_t lineOrBytecode, uint32_t funcImportIndex);
    MOZ_MUST_USE bool emitCall();
    MOZ_MUST_USE bool emitOldCallImport();
    MOZ_MUST_USE bool emitCallIndirect(bool oldStyle);
    MOZ_MUST_USE bool emitCommonMathCall(uint32_t lineOrBytecode, SymbolicAddress callee,
                                         ValTypeVector& signature, ExprType retType);
    MOZ_MUST_USE bool emitUnaryMathBuiltinCall(SymbolicAddress callee, ValType operandType);
    MOZ_MUST_USE bool emitBinaryMathBuiltinCall(SymbolicAddress callee, ValType operandType);
#ifdef QUOT_REM_I64_CALLOUT
    MOZ_MUST_USE bool emitDivOrModI64BuiltinCall(SymbolicAddress callee, ValType operandType);
#endif
    MOZ_MUST_USE bool emitGetLocal();
    MOZ_MUST_USE bool emitSetLocal();
    MOZ_MUST_USE bool emitTeeLocal();
    MOZ_MUST_USE bool emitGetGlobal();
    MOZ_MUST_USE bool emitSetGlobal();
    MOZ_MUST_USE bool emitTeeGlobal();
    MOZ_MUST_USE bool emitLoad(ValType type, Scalar::Type viewType);
    MOZ_MUST_USE bool emitStore(ValType resultType, Scalar::Type viewType);
    MOZ_MUST_USE bool emitTeeStore(ValType resultType, Scalar::Type viewType);
    MOZ_MUST_USE bool emitTeeStoreWithCoercion(ValType resultType, Scalar::Type viewType);
    MOZ_MUST_USE bool emitSelect();

    void endBlock(ExprType type, bool isFunctionBody);
    void endLoop(ExprType type);
    void endIfThen();
    void endIfThenElse(ExprType type);

    void doReturn(ExprType returnType);
    void pushReturned(const FunctionCall& call, ExprType type);

    void emitCompareI32(JSOp compareOp, MCompare::CompareType compareType);
    void emitCompareI64(JSOp compareOp, MCompare::CompareType compareType);
    void emitCompareF32(JSOp compareOp, MCompare::CompareType compareType);
    void emitCompareF64(JSOp compareOp, MCompare::CompareType compareType);

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
#ifndef QUOT_REM_I64_CALLOUT
    void emitQuotientI64();
    void emitQuotientU64();
    void emitRemainderI64();
    void emitRemainderU64();
#endif
    void emitDivideF32();
    void emitDivideF64();
    void emitMinI32();
    void emitMaxI32();
    void emitMinMaxI32(Assembler::Condition cond);
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
    void emitBitNotI32();
    void emitAbsI32();
    void emitAbsF32();
    void emitAbsF64();
    void emitNegateI32();
    void emitNegateF32();
    void emitNegateF64();
    void emitSqrtF32();
    void emitSqrtF64();
    template<bool isUnsigned> MOZ_MUST_USE bool emitTruncateF32ToI32();
    template<bool isUnsigned> MOZ_MUST_USE bool emitTruncateF64ToI32();
#ifndef FLOAT_TO_I64_CALLOUT
    template<bool isUnsigned> MOZ_MUST_USE bool emitTruncateF32ToI64();
    template<bool isUnsigned> MOZ_MUST_USE bool emitTruncateF64ToI64();
#endif
    void emitWrapI64ToI32();
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
#ifndef I64_TO_FLOAT_CALLOUT
    void emitConvertI64ToF32();
    void emitConvertU64ToF32();
    void emitConvertI64ToF64();
    void emitConvertU64ToF64();
#endif
    void emitReinterpretI32AsF32();
    void emitReinterpretI64AsF64();
    MOZ_MUST_USE bool emitGrowMemory();
    MOZ_MUST_USE bool emitCurrentMemory();
#ifdef I64_TO_FLOAT_CALLOUT
    MOZ_MUST_USE bool emitConvertInt64ToFloatingCallout(SymbolicAddress callee, ValType operandType,
                                                        ValType resultType);
#endif
#ifdef FLOAT_TO_I64_CALLOUT
    MOZ_MUST_USE bool emitConvertFloatingToInt64Callout(SymbolicAddress callee, ValType operandType,
                                                        ValType resultType);
#endif
};

void
BaseCompiler::emitAddI32()
{
    int32_t c;
    if (popConstI32(c)) {
        RegI32 r = popI32();
        masm.add32(Imm32(c), r.reg);
        pushI32(r);
    } else {
        RegI32 r0, r1;
        pop2xI32(&r0, &r1);
        masm.add32(r1.reg, r0.reg);
        freeI32(r1);
        pushI32(r0);
    }
}

void
BaseCompiler::emitAddI64()
{
    // TODO / OPTIMIZE: Ditto check for constant here
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    masm.add64(r1.reg, r0.reg);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitAddF64()
{
    // TODO / OPTIMIZE: Ditto check for constant here
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    masm.addDouble(r1.reg, r0.reg);
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitAddF32()
{
    // TODO / OPTIMIZE: Ditto check for constant here
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    masm.addFloat32(r1.reg, r0.reg);
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitSubtractI32()
{
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    masm.sub32(r1.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitSubtractI64()
{
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    subtractI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitSubtractF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    masm.subFloat32(r1.reg, r0.reg);
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitSubtractF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    masm.subDouble(r1.reg, r0.reg);
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitMultiplyI32()
{
    // TODO / OPTIMIZE: Multiplication by constant is common (bug 1275442)
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // srcDest must be eax, and edx will be clobbered.
    need2xI32(specific_eax, specific_edx);
    r1 = popI32();
    r0 = popI32ToSpecific(specific_eax);
    freeI32(specific_edx);
#else
    pop2xI32(&r0, &r1);
#endif
    masm.mul32(r1.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitMultiplyI64()
{
    // TODO / OPTIMIZE: Multiplication by constant is common (bug 1275442)
    RegI64 r0, r1;
    RegI32 temp;
#if defined(JS_CODEGEN_X64)
    // srcDest must be rax, and rdx will be clobbered.
    need2xI64(specific_rax, specific_rdx);
    r1 = popI64();
    r0 = popI64ToSpecific(specific_rax);
    freeI64(specific_rdx);
#elif defined(JS_CODEGEN_X86)
    need2xI32(specific_eax, specific_edx);
    r1 = popI64();
    r0 = popI64ToSpecific(RegI64(Register64(specific_edx.reg, specific_eax.reg)));
    temp = needI32();
#else
    pop2xI64(&r0, &r1);
    temp = needI32();
#endif
    multiplyI64(r1, r0, temp);
    if (temp.reg != Register::Invalid())
        freeI32(temp);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitMultiplyF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    masm.mulFloat32(r1.reg, r0.reg);
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitMultiplyF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    masm.mulDouble(r1.reg, r0.reg);
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitQuotientI32()
{
    // TODO / OPTIMIZE: Fast case if lhs >= 0 and rhs is power of two.
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // srcDest must be eax, and edx will be clobbered.
    need2xI32(specific_eax, specific_edx);
    r1 = popI32();
    r0 = popI32ToSpecific(specific_eax);
    freeI32(specific_edx);
#else
    pop2xI32(&r0, &r1);
#endif

    Label done;
    checkDivideByZeroI32(r1, r0, &done);
    checkDivideSignedOverflowI32(r1, r0, &done, ZeroOnOverflow(false));
    masm.quotient32(r1.reg, r0.reg, IsUnsigned(false));
    masm.bind(&done);

    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitQuotientU32()
{
    // TODO / OPTIMIZE: Fast case if lhs >= 0 and rhs is power of two.
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // srcDest must be eax, and edx will be clobbered.
    need2xI32(specific_eax, specific_edx);
    r1 = popI32();
    r0 = popI32ToSpecific(specific_eax);
    freeI32(specific_edx);
#else
    pop2xI32(&r0, &r1);
#endif

    Label done;
    checkDivideByZeroI32(r1, r0, &done);
    masm.quotient32(r1.reg, r0.reg, IsUnsigned(true));
    masm.bind(&done);

    freeI32(r1);
    pushI32(r0);
}

#ifndef QUOT_REM_I64_CALLOUT
void
BaseCompiler::emitQuotientI64()
{
# ifdef JS_PUNBOX64
    RegI64 r0, r1;
#  ifdef JS_CODEGEN_X64
    // srcDest must be rax, and rdx will be clobbered.
    need2xI64(specific_rax, specific_rdx);
    r1 = popI64();
    r0 = popI64ToSpecific(specific_rax);
    freeI64(specific_rdx);
#  else
    pop2xI64(&r0, &r1);
#  endif
    quotientI64(r1, r0, IsUnsigned(false));
    freeI64(r1);
    pushI64(r0);
# else
    MOZ_CRASH("BaseCompiler platform hook: emitQuotientI64");
# endif
}

void
BaseCompiler::emitQuotientU64()
{
# ifdef JS_PUNBOX64
    RegI64 r0, r1;
#  ifdef JS_CODEGEN_X64
    // srcDest must be rax, and rdx will be clobbered.
    need2xI64(specific_rax, specific_rdx);
    r1 = popI64();
    r0 = popI64ToSpecific(specific_rax);
    freeI64(specific_rdx);
#  else
    pop2xI64(&r0, &r1);
#  endif
    quotientI64(r1, r0, IsUnsigned(true));
    freeI64(r1);
    pushI64(r0);
# else
    MOZ_CRASH("BaseCompiler platform hook: emitQuotientU64");
# endif
}
#endif

void
BaseCompiler::emitRemainderI32()
{
    // TODO / OPTIMIZE: Fast case if lhs >= 0 and rhs is power of two.
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // srcDest must be eax, and edx will be clobbered.
    need2xI32(specific_eax, specific_edx);
    r1 = popI32();
    r0 = popI32ToSpecific(specific_eax);
    freeI32(specific_edx);
#else
    pop2xI32(&r0, &r1);
#endif

    Label done;
    checkDivideByZeroI32(r1, r0, &done);
    checkDivideSignedOverflowI32(r1, r0, &done, ZeroOnOverflow(true));
    masm.remainder32(r1.reg, r0.reg, IsUnsigned(false));
    masm.bind(&done);

    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitRemainderU32()
{
    // TODO / OPTIMIZE: Fast case if lhs >= 0 and rhs is power of two.
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // srcDest must be eax, and edx will be clobbered.
    need2xI32(specific_eax, specific_edx);
    r1 = popI32();
    r0 = popI32ToSpecific(specific_eax);
    freeI32(specific_edx);
#else
    pop2xI32(&r0, &r1);
#endif

    Label done;
    checkDivideByZeroI32(r1, r0, &done);
    masm.remainder32(r1.reg, r0.reg, IsUnsigned(true));
    masm.bind(&done);

    freeI32(r1);
    pushI32(r0);
}

#ifndef QUOT_REM_I64_CALLOUT
void
BaseCompiler::emitRemainderI64()
{
# ifdef JS_PUNBOX64
    RegI64 r0, r1;
#  ifdef JS_CODEGEN_X64
    need2xI64(specific_rax, specific_rdx);
    r1 = popI64();
    r0 = popI64ToSpecific(specific_rax);
    freeI64(specific_rdx);
#  else
    pop2xI64(&r0, &r1);
#  endif
    remainderI64(r1, r0, IsUnsigned(false));
    freeI64(r1);
    pushI64(r0);
# else
    MOZ_CRASH("BaseCompiler platform hook: emitRemainderI64");
# endif
}

void
BaseCompiler::emitRemainderU64()
{
# ifdef JS_PUNBOX64
    RegI64 r0, r1;
#  ifdef JS_CODEGEN_X64
    need2xI64(specific_rax, specific_rdx);
    r1 = popI64();
    r0 = popI64ToSpecific(specific_rax);
    freeI64(specific_rdx);
#  else
    pop2xI64(&r0, &r1);
#  endif
    remainderI64(r1, r0, IsUnsigned(true));
    freeI64(r1);
    pushI64(r0);
# else
    MOZ_CRASH("BaseCompiler platform hook: emitRemainderU64");
# endif
}
#endif

void
BaseCompiler::emitDivideF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    masm.divFloat32(r1.reg, r0.reg);
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitDivideF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    masm.divDouble(r1.reg, r0.reg);
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitMinI32()
{
    emitMinMaxI32(Assembler::LessThan);
}

void
BaseCompiler::emitMaxI32()
{
    emitMinMaxI32(Assembler::GreaterThan);
}

void
BaseCompiler::emitMinMaxI32(Assembler::Condition cond)
{
    Label done;
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    // TODO / OPTIMIZE: Use conditional move on some platforms?
    masm.branch32(cond, r0.reg, r1.reg, &done);
    moveI32(r1, r0);
    masm.bind(&done);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitMinF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    if (!isCompilingAsmJS()) {
        // Convert signaling NaN to quiet NaNs.
        // TODO / OPTIMIZE: Don't do this if one of the operands is known to
        // be a constant.
        ScratchF32 zero(*this);
        masm.loadConstantFloat32(0.f, zero);
        masm.subFloat32(zero, r0.reg);
        masm.subFloat32(zero, r1.reg);
    }
    masm.minFloat32(r1.reg, r0.reg, HandleNaNSpecially(true));
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitMaxF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    if (!isCompilingAsmJS()) {
        // Convert signaling NaN to quiet NaNs.
        // TODO / OPTIMIZE: see comment in emitMinF32.
        ScratchF32 zero(*this);
        masm.loadConstantFloat32(0.f, zero);
        masm.subFloat32(zero, r0.reg);
        masm.subFloat32(zero, r1.reg);
    }
    masm.maxFloat32(r1.reg, r0.reg, HandleNaNSpecially(true));
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitMinF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    if (!isCompilingAsmJS()) {
        // Convert signaling NaN to quiet NaNs.
        // TODO / OPTIMIZE: see comment in emitMinF32.
        ScratchF64 zero(*this);
        masm.loadConstantDouble(0, zero);
        masm.subDouble(zero, r0.reg);
        masm.subDouble(zero, r1.reg);
    }
    masm.minDouble(r1.reg, r0.reg, HandleNaNSpecially(true));
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitMaxF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    if (!isCompilingAsmJS()) {
        // Convert signaling NaN to quiet NaNs.
        // TODO / OPTIMIZE: see comment in emitMinF32.
        ScratchF64 zero(*this);
        masm.loadConstantDouble(0, zero);
        masm.subDouble(zero, r0.reg);
        masm.subDouble(zero, r1.reg);
    }
    masm.maxDouble(r1.reg, r0.reg, HandleNaNSpecially(true));
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitCopysignF32()
{
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    RegI32 i0 = needI32();
    RegI32 i1 = needI32();
    masm.moveFloat32ToGPR(r0.reg, i0.reg);
    masm.moveFloat32ToGPR(r1.reg, i1.reg);
    masm.and32(Imm32(INT32_MAX), i0.reg);
    masm.and32(Imm32(INT32_MIN), i1.reg);
    masm.or32(i1.reg, i0.reg);
    masm.moveGPRToFloat32(i0.reg, r0.reg);
    freeI32(i0);
    freeI32(i1);
    freeF32(r1);
    pushF32(r0);
}

void
BaseCompiler::emitCopysignF64()
{
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    RegI64 x0 = needI64();
    RegI64 x1 = needI64();
    reinterpretF64AsI64(r0, x0);
    reinterpretF64AsI64(r1, x1);
    masm.and64(Imm64(INT64_MAX), x0.reg);
    masm.and64(Imm64(INT64_MIN), x1.reg);
    masm.or64(x1.reg, x0.reg);
    reinterpretI64AsF64(x0, r0);
    freeI64(x0);
    freeI64(x1);
    freeF64(r1);
    pushF64(r0);
}

void
BaseCompiler::emitOrI32()
{
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    masm.or32(r1.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitOrI64()
{
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    orI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitAndI32()
{
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    masm.and32(r1.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitAndI64()
{
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    andI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitXorI32()
{
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    masm.xor32(r1.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitXorI64()
{
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    xorI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitShlI32()
{
    int32_t c;
    if (popConstI32(c)) {
        RegI32 r = popI32();
        masm.lshift32(Imm32(c & 31), r.reg);
        pushI32(r);
    } else {
        RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        r1 = popI32(specific_ecx);
        r0 = popI32();
#else
        pop2xI32(&r0, &r1);
#endif
        lshiftI32(r1, r0);
        freeI32(r1);
        pushI32(r0);
    }
}

void
BaseCompiler::emitShlI64()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI64 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needI32(specific_ecx);
    r1 = fromI32(specific_ecx);
    r1 = popI64ToSpecific(r1);
    r0 = popI64();
#else
    pop2xI64(&r0, &r1);
#endif
    lshiftI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitShrI32()
{
    int32_t c;
    if (popConstI32(c)) {
        RegI32 r = popI32();
        masm.rshift32Arithmetic(Imm32(c & 31), r.reg);
        pushI32(r);
    } else {
        RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        r1 = popI32(specific_ecx);
        r0 = popI32();
#else
        pop2xI32(&r0, &r1);
#endif
        rshiftI32(r1, r0);
        freeI32(r1);
        pushI32(r0);
    }
}

void
BaseCompiler::emitShrI64()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI64 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needI32(specific_ecx);
    r1 = fromI32(specific_ecx);
    r1 = popI64ToSpecific(r1);
    r0 = popI64();
#else
    pop2xI64(&r0, &r1);
#endif
    rshiftI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitShrU32()
{
    int32_t c;
    if (popConstI32(c)) {
        RegI32 r = popI32();
        masm.rshift32(Imm32(c & 31), r.reg);
        pushI32(r);
    } else {
        RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
        r1 = popI32(specific_ecx);
        r0 = popI32();
#else
        pop2xI32(&r0, &r1);
#endif
        rshiftIU32(r1, r0);
        freeI32(r1);
        pushI32(r0);
    }
}

void
BaseCompiler::emitShrU64()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI64 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needI32(specific_ecx);
    r1 = fromI32(specific_ecx);
    r1 = popI64ToSpecific(r1);
    r0 = popI64();
#else
    pop2xI64(&r0, &r1);
#endif
    rshiftU64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitRotrI32()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    r1 = popI32(specific_ecx);
    r0 = popI32();
#else
    pop2xI32(&r0, &r1);
#endif
    masm.rotateRight(r1.reg, r0.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitRotrI64()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI64 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needI32(specific_ecx);
    r1 = fromI32(specific_ecx);
    r1 = popI64ToSpecific(r1);
    r0 = popI64();
#else
    pop2xI64(&r0, &r1);
#endif
    rotateRightI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitRotlI32()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI32 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    r1 = popI32(specific_ecx);
    r0 = popI32();
#else
    pop2xI32(&r0, &r1);
#endif
    masm.rotateLeft(r1.reg, r0.reg, r0.reg);
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitRotlI64()
{
    // TODO / OPTIMIZE: Constant rhs
    RegI64 r0, r1;
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    needI32(specific_ecx);
    r1 = fromI32(specific_ecx);
    r1 = popI64ToSpecific(r1);
    r0 = popI64();
#else
    pop2xI64(&r0, &r1);
#endif
    rotateLeftI64(r1, r0);
    freeI64(r1);
    pushI64(r0);
}

void
BaseCompiler::emitEqzI32()
{
    // TODO / OPTIMIZE: Boolean evaluation for control
    RegI32 r0 = popI32();
    masm.cmp32Set(Assembler::Equal, r0.reg, Imm32(0), r0.reg);
    pushI32(r0);
}

void
BaseCompiler::emitEqzI64()
{
    // TODO / OPTIMIZE: Boolean evaluation for control
    // TODO / OPTIMIZE: Avoid the temp register
    RegI64 r0 = popI64();
    RegI64 r1 = needI64();
    setI64(0, r1);
    RegI32 i0 = fromI64(r0);
    cmp64Set(Assembler::Equal, r0, r1, i0);
    freeI64(r1);
    freeI64Except(r0, i0);
    pushI32(i0);
}

void
BaseCompiler::emitClzI32()
{
    RegI32 r0 = popI32();
    masm.clz32(r0.reg, r0.reg, IsKnownNotZero(false));
    pushI32(r0);
}

void
BaseCompiler::emitClzI64()
{
    RegI64 r0 = popI64();
    clzI64(r0);
    pushI64(r0);
}

void
BaseCompiler::emitCtzI32()
{
    RegI32 r0 = popI32();
    masm.ctz32(r0.reg, r0.reg, IsKnownNotZero(false));
    pushI32(r0);
}

void
BaseCompiler::emitCtzI64()
{
    RegI64 r0 = popI64();
    ctzI64(r0);
    pushI64(r0);
}

void
BaseCompiler::emitPopcntI32()
{
    RegI32 r0 = popI32();
    if (popcnt32NeedsTemp()) {
        RegI32 tmp = needI32();
        popcntI32(r0, tmp);
        freeI32(tmp);
    } else {
        popcntI32(r0, invalidI32());
    }
    pushI32(r0);
}

void
BaseCompiler::emitPopcntI64()
{
    RegI64 r0 = popI64();
    if (popcnt64NeedsTemp()) {
        RegI32 tmp = needI32();
        popcntI64(r0, tmp);
        freeI32(tmp);
    } else {
        popcntI64(r0, invalidI32());
    }
    pushI64(r0);
}

void
BaseCompiler::emitBitNotI32()
{
    RegI32 r0 = popI32();
    masm.not32(r0.reg);
    pushI32(r0);
}

void
BaseCompiler::emitAbsI32()
{
    // TODO / OPTIMIZE: Use conditional move on some platforms?
    Label nonnegative;
    RegI32 r0 = popI32();
    masm.branch32(Assembler::GreaterThanOrEqual, r0.reg, Imm32(0), &nonnegative);
    masm.neg32(r0.reg);
    masm.bind(&nonnegative);
    pushI32(r0);
}

void
BaseCompiler::emitAbsF32()
{
    RegF32 r0 = popF32();
    masm.absFloat32(r0.reg, r0.reg);
    pushF32(r0);
}

void
BaseCompiler::emitAbsF64()
{
    RegF64 r0 = popF64();
    masm.absDouble(r0.reg, r0.reg);
    pushF64(r0);
}

void
BaseCompiler::emitNegateI32()
{
    RegI32 r0 = popI32();
    masm.neg32(r0.reg);
    pushI32(r0);
}

void
BaseCompiler::emitNegateF32()
{
    RegF32 r0 = popF32();
    masm.negateFloat(r0.reg);
    pushF32(r0);
}

void
BaseCompiler::emitNegateF64()
{
    RegF64 r0 = popF64();
    masm.negateDouble(r0.reg);
    pushF64(r0);
}

void
BaseCompiler::emitSqrtF32()
{
    RegF32 r0 = popF32();
    masm.sqrtFloat32(r0.reg, r0.reg);
    pushF32(r0);
}

void
BaseCompiler::emitSqrtF64()
{
    RegF64 r0 = popF64();
    masm.sqrtDouble(r0.reg, r0.reg);
    pushF64(r0);
}

template<bool isUnsigned>
bool
BaseCompiler::emitTruncateF32ToI32()
{
    RegF32 r0 = popF32();
    RegI32 i0 = needI32();
    if (!truncateF32ToI32(r0, i0, isUnsigned))
        return false;
    freeF32(r0);
    pushI32(i0);
    return true;
}

template<bool isUnsigned>
bool
BaseCompiler::emitTruncateF64ToI32()
{
    RegF64 r0 = popF64();
    RegI32 i0 = needI32();
    if (!truncateF64ToI32(r0, i0, isUnsigned))
        return false;
    freeF64(r0);
    pushI32(i0);
    return true;
}

#ifndef FLOAT_TO_I64_CALLOUT
template<bool isUnsigned>
bool
BaseCompiler::emitTruncateF32ToI64()
{
    RegF32 r0 = popF32();
    RegI64 x0 = needI64();
    if (isUnsigned) {
        RegF64 tmp = needF64();
        if (!truncateF32ToI64(r0, x0, isUnsigned, tmp))
            return false;
        freeF64(tmp);
    } else {
        if (!truncateF32ToI64(r0, x0, isUnsigned, invalidF64()))
            return false;
    }
    freeF32(r0);
    pushI64(x0);
    return true;
}

template<bool isUnsigned>
bool
BaseCompiler::emitTruncateF64ToI64()
{
    RegF64 r0 = popF64();
    RegI64 x0 = needI64();
    if (isUnsigned) {
        RegF64 tmp = needF64();
        if (!truncateF64ToI64(r0, x0, isUnsigned, tmp))
            return false;
        freeF64(tmp);
    } else {
        if (!truncateF64ToI64(r0, x0, isUnsigned, invalidF64()))
            return false;
    }
    freeF64(r0);
    pushI64(x0);
    return true;
}
#endif

void
BaseCompiler::emitWrapI64ToI32()
{
    RegI64 r0 = popI64();
    RegI32 i0 = fromI64(r0);
    wrapI64ToI32(r0, i0);
    freeI64Except(r0, i0);
    pushI32(i0);
}

void
BaseCompiler::emitExtendI32ToI64()
{
#if defined(JS_CODEGEN_X86)
    need2xI32(specific_edx, specific_eax);
    RegI32 r0 = popI32ToSpecific(specific_eax);
    RegI64 x0 = RegI64(Register64(specific_edx.reg, specific_eax.reg));
#else
    RegI32 r0 = popI32();
    RegI64 x0 = fromI32(r0);
#endif
    extendI32ToI64(r0, x0);
    pushI64(x0);
    // Note: no need to free r0, since it is part of x0
}

void
BaseCompiler::emitExtendU32ToI64()
{
    RegI32 r0 = popI32();
    RegI64 x0 = fromI32(r0);
    extendU32ToI64(r0, x0);
    pushI64(x0);
    // Note: no need to free r0, since it is part of x0
}

void
BaseCompiler::emitReinterpretF32AsI32()
{
    RegF32 r0 = popF32();
    RegI32 i0 = needI32();
    masm.moveFloat32ToGPR(r0.reg, i0.reg);
    freeF32(r0);
    pushI32(i0);
}

void
BaseCompiler::emitReinterpretF64AsI64()
{
    RegF64 r0 = popF64();
    RegI64 x0 = needI64();
    reinterpretF64AsI64(r0, x0);
    freeF64(r0);
    pushI64(x0);
}

void
BaseCompiler::emitConvertF64ToF32()
{
    RegF64 r0 = popF64();
    RegF32 f0 = needF32();
    masm.convertDoubleToFloat32(r0.reg, f0.reg);
    freeF64(r0);
    pushF32(f0);
}

void
BaseCompiler::emitConvertI32ToF32()
{
    RegI32 r0 = popI32();
    RegF32 f0 = needF32();
    masm.convertInt32ToFloat32(r0.reg, f0.reg);
    freeI32(r0);
    pushF32(f0);
}

void
BaseCompiler::emitConvertU32ToF32()
{
    RegI32 r0 = popI32();
    RegF32 f0 = needF32();
    masm.convertUInt32ToFloat32(r0.reg, f0.reg);
    freeI32(r0);
    pushF32(f0);
}

#ifndef I64_TO_FLOAT_CALLOUT
void
BaseCompiler::emitConvertI64ToF32()
{
    RegI64 r0 = popI64();
    RegF32 f0 = needF32();
    convertI64ToF32(r0, IsUnsigned(false), f0, RegI32());
    freeI64(r0);
    pushF32(f0);
}

void
BaseCompiler::emitConvertU64ToF32()
{
    RegI64 r0 = popI64();
    RegF32 f0 = needF32();
    RegI32 temp;
    if (convertI64ToFloatNeedsTemp(IsUnsigned(true)))
        temp = needI32();
    convertI64ToF32(r0, IsUnsigned(true), f0, temp);
    if (temp.reg != Register::Invalid())
        freeI32(temp);
    freeI64(r0);
    pushF32(f0);
}
#endif

void
BaseCompiler::emitConvertF32ToF64()
{
    RegF32 r0 = popF32();
    RegF64 d0 = needF64();
    masm.convertFloat32ToDouble(r0.reg, d0.reg);
    freeF32(r0);
    pushF64(d0);
}

void
BaseCompiler::emitConvertI32ToF64()
{
    RegI32 r0 = popI32();
    RegF64 d0 = needF64();
    masm.convertInt32ToDouble(r0.reg, d0.reg);
    freeI32(r0);
    pushF64(d0);
}

void
BaseCompiler::emitConvertU32ToF64()
{
    RegI32 r0 = popI32();
    RegF64 d0 = needF64();
    masm.convertUInt32ToDouble(r0.reg, d0.reg);
    freeI32(r0);
    pushF64(d0);
}

#ifndef I64_TO_FLOAT_CALLOUT
void
BaseCompiler::emitConvertI64ToF64()
{
    RegI64 r0 = popI64();
    RegF64 d0 = needF64();
    convertI64ToF64(r0, IsUnsigned(false), d0, RegI32());
    freeI64(r0);
    pushF64(d0);
}

void
BaseCompiler::emitConvertU64ToF64()
{
    RegI64 r0 = popI64();
    RegF64 d0 = needF64();
    RegI32 temp;
    if (convertI64ToFloatNeedsTemp(IsUnsigned(true)))
        temp = needI32();
    convertI64ToF64(r0, IsUnsigned(true), d0, temp);
    if (temp.reg != Register::Invalid())
        freeI32(temp);
    freeI64(r0);
    pushF64(d0);
}
#endif

void
BaseCompiler::emitReinterpretI32AsF32()
{
    RegI32 r0 = popI32();
    RegF32 f0 = needF32();
    masm.moveGPRToFloat32(r0.reg, f0.reg);
    freeI32(r0);
    pushF32(f0);
}

void
BaseCompiler::emitReinterpretI64AsF64()
{
    RegI64 r0 = popI64();
    RegF64 d0 = needF64();
    reinterpretI64AsF64(r0, d0);
    freeI64(r0);
    pushF64(d0);
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

bool
BaseCompiler::emitBlock()
{
    if (!iter_.readBlock())
        return false;

    UniquePooledLabel blockEnd(newLabel());
    if (!blockEnd)
        return false;

    if (!deadCode_)
        sync();                    // Simplifies branching out from block

    return pushControl(&blockEnd);
}

void
BaseCompiler::endBlock(ExprType type, bool isFunctionBody)
{
    Control& block = controlItem(0);

    // Save the value.
    AnyReg r;
    if (!deadCode_ && !IsVoid(type))
        r = popJoinReg();

    // Leave the block.
    popStackOnBlockExit(block.framePushed);

    // Bind after cleanup: branches out will have popped the stack.
    if (block.label->used()) {
        masm.bind(block.label);
        if (deadCode_ && !IsVoid(type))
            r = allocJoinReg(type);
        deadCode_ = false;
    }

    MOZ_ASSERT(stk_.length() == block.stackSize);

    // Retain the value stored in joinReg by all paths.
    if (!deadCode_) {
        if (!IsVoid(type))
            pushJoinReg(r);

        if (isFunctionBody)
            doReturn(func_.sig().ret());
    }

    popControl();
}

bool
BaseCompiler::emitLoop()
{
    if (!iter_.readLoop())
        return false;

    UniquePooledLabel blockCont(newLabel());
    if (!blockCont)
        return false;

    if (!deadCode_)
        sync();                    // Simplifies branching out from block

    if (!pushControl(&blockCont))
        return false;

    if (!deadCode_) {
        masm.bind(controlItem(0).label);
        addInterruptCheck();
    }

    return true;
}

void
BaseCompiler::endLoop(ExprType type)
{
    Control& block = controlItem(0);

    AnyReg r;
    if (!deadCode_ && !IsVoid(type))
        r = popJoinReg();

    popStackOnBlockExit(block.framePushed);

    MOZ_ASSERT(stk_.length() == block.stackSize);

    popControl();

    // Retain the value stored in joinReg by all paths.
    if (!deadCode_ && !IsVoid(type))
        pushJoinReg(r);
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

bool
BaseCompiler::emitIf()
{
    Nothing unused_cond;
    if (!iter_.readIf(&unused_cond))
        return false;

    UniquePooledLabel endLabel(newLabel());
    if (!endLabel)
        return false;

    UniquePooledLabel elseLabel(newLabel());
    if (!elseLabel)
        return false;

    RegI32 rc;
    if (!deadCode_) {
        rc = popI32();
        sync();                    // Simplifies branching out from the arms
    }

    if (!pushControl(&endLabel, &elseLabel))
        return false;

    if (!deadCode_) {
        masm.branch32(Assembler::Equal, rc.reg, Imm32(0), controlItem(0).otherLabel);
        freeI32(rc);
    }

    return true;
}

void
BaseCompiler::endIfThen()
{
    Control& ifThen = controlItem(0);

    popStackOnBlockExit(ifThen.framePushed);

    if (ifThen.otherLabel->used())
        masm.bind(ifThen.otherLabel);

    if (ifThen.label->used())
        masm.bind(ifThen.label);

    deadCode_ = ifThen.deadOnArrival;

    MOZ_ASSERT(stk_.length() == ifThen.stackSize);

    popControl();
}

bool
BaseCompiler::emitElse()
{
    ExprType thenType;
    Nothing unused_thenValue;
    if (!iter_.readElse(&thenType, &unused_thenValue))
        return false;

    Control& ifThenElse = controlItem(0);

    // See comment in endIfThenElse, below.

    // Exit the "then" branch.

    ifThenElse.deadThenBranch = deadCode_;

    AnyReg r;
    if (!deadCode_ && !IsVoid(thenType))
        r = popJoinReg();

    popStackOnBlockExit(ifThenElse.framePushed);

    if (!deadCode_)
        masm.jump(ifThenElse.label);

    if (ifThenElse.otherLabel->used())
        masm.bind(ifThenElse.otherLabel);

    // Reset to the "else" branch.

    MOZ_ASSERT(stk_.length() == ifThenElse.stackSize);

    if (!deadCode_ && !IsVoid(thenType))
        freeJoinReg(r);

    deadCode_ = ifThenElse.deadOnArrival;

    return true;
}

void
BaseCompiler::endIfThenElse(ExprType type)
{
    Control& ifThenElse = controlItem(0);

    // The expression type is not a reliable guide to what we'll find
    // on the stack, we could have (if E (i32.const 1) (unreachable))
    // in which case the "else" arm is AnyType but the type of the
    // full expression is I32.  So restore whatever's there, not what
    // we want to find there.  The "then" arm has the same constraint.

    AnyReg r;
    if (!deadCode_ && !IsVoid(type))
        r = popJoinReg();

    popStackOnBlockExit(ifThenElse.framePushed);

    if (ifThenElse.label->used())
        masm.bind(ifThenElse.label);

    if (!ifThenElse.deadOnArrival &&
        (!ifThenElse.deadThenBranch || !deadCode_ || ifThenElse.label->bound())) {
        if (deadCode_ && !IsVoid(type))
            r = allocJoinReg(type);
        deadCode_ = false;
    }

    MOZ_ASSERT(stk_.length() == ifThenElse.stackSize);

    popControl();

    if (!deadCode_ && !IsVoid(type))
        pushJoinReg(r);
}

bool
BaseCompiler::emitEnd()
{
    LabelKind kind;
    ExprType type;
    Nothing unused_value;
    if (!iter_.readEnd(&kind, &type, &unused_value))
        return false;

    switch (kind) {
      case LabelKind::Block: endBlock(type, iter_.controlStackEmpty()); break;
      case LabelKind::Loop:  endLoop(type); break;
      case LabelKind::UnreachableThen:
      case LabelKind::Then:  endIfThen(); break;
      case LabelKind::Else:  endIfThenElse(type); break;
    }

    return true;
}

bool
BaseCompiler::emitBr()
{
    uint32_t relativeDepth;
    ExprType type;
    Nothing unused_value;
    if (!iter_.readBr(&relativeDepth, &type, &unused_value))
        return false;

    if (deadCode_)
        return true;

    Control& target = controlItem(relativeDepth);

    // Save any value in the designated join register, where the
    // normal block exit code will also leave it.

    AnyReg r;
    if (!IsVoid(type))
        r = popJoinReg();

    popStackBeforeBranch(target.framePushed);
    masm.jump(target.label);

    // The register holding the join value is free for the remainder
    // of this block.

    if (!IsVoid(type))
        freeJoinReg(r);

    deadCode_ = true;

    popValueStackTo(ctl_.back().stackSize);

    return true;
}

bool
BaseCompiler::emitBrIf()
{
    uint32_t relativeDepth;
    ExprType type;
    Nothing unused_value, unused_condition;
    if (!iter_.readBrIf(&relativeDepth, &type, &unused_value, &unused_condition))
        return false;

    if (deadCode_)
        return true;

    Control& target = controlItem(relativeDepth);

    // TODO / OPTIMIZE: Optimize boolean evaluation for control by
    // allowing a conditional expression to be left on the stack and
    // reified here as part of the branch instruction.

    // Don't use joinReg for rc
    maybeReserveJoinRegI(type);

    // Condition value is on top, always I32.
    RegI32 rc = popI32();

    maybeUnreserveJoinRegI(type);

    // Save any value in the designated join register, where the
    // normal block exit code will also leave it.
    AnyReg r;
    if (!IsVoid(type))
        r = popJoinReg();

    masm.branch32(Assembler::NotEqual, rc.reg, Imm32(0), target.label);

    // This register is free in the remainder of the block.
    freeI32(rc);

    // br_if returns its value(s).
    if (!IsVoid(type))
        pushJoinReg(r);

    return true;
}

bool
BaseCompiler::emitBrTable()
{
    uint32_t tableLength;
    ExprType type;
    Nothing unused_value, unused_index;
    if (!iter_.readBrTable(&tableLength, &type, &unused_value, &unused_index))
        return false;

    LabelVector stubs;
    if (!stubs.reserve(tableLength+1))
        return false;

    Uint32Vector depths;
    if (!depths.reserve(tableLength))
        return false;

    for (size_t i = 0; i < tableLength; ++i) {
        uint32_t depth;
        if (!iter_.readBrTableEntry(&type, &unused_value, &depth))
            return false;
        depths.infallibleAppend(depth);
    }

    uint32_t defaultDepth;
    if (!iter_.readBrTableDefault(&type, &unused_value, &defaultDepth))
        return false;

    if (deadCode_)
        return true;

    // Don't use joinReg for rc
    maybeReserveJoinRegI(type);

    // Table switch value always on top.
    RegI32 rc = popI32();

    maybeUnreserveJoinRegI(type);

    AnyReg r;
    if (!IsVoid(type))
        r = popJoinReg();

    Label dispatchCode;
    masm.branch32(Assembler::Below, rc.reg, Imm32(tableLength), &dispatchCode);

    // This is the out-of-range stub.  rc is dead here but we don't need it.

    popStackBeforeBranch(controlItem(defaultDepth).framePushed);
    masm.jump(controlItem(defaultDepth).label);

    // Emit stubs.  rc is dead in all of these but we don't need it.

    for (uint32_t i = 0; i < tableLength; i++) {
        PooledLabel* stubLabel = newLabel();
        // The labels in the vector are in the TempAllocator and will
        // be freed by and by.
        if (!stubLabel)
            return false;
        stubs.infallibleAppend(stubLabel);
        masm.bind(stubLabel);
        uint32_t k = depths[i];
        popStackBeforeBranch(controlItem(k).framePushed);
        masm.jump(controlItem(k).label);
    }

    // Emit table.

    Label theTable;
    masm.bind(&theTable);
    jumpTable(stubs);

    // Emit indirect jump.  rc is live here.

    masm.bind(&dispatchCode);
    tableSwitch(&theTable, rc);

    deadCode_ = true;

    // Clean up.

    freeI32(rc);
    if (!IsVoid(type))
        freeJoinReg(r);

    for (uint32_t i = 0; i < tableLength; i++)
        freeLabel(stubs[i]);

    popValueStackTo(ctl_.back().stackSize);

    return true;
}

void
BaseCompiler::doReturn(ExprType type)
{
    switch (type) {
      case ExprType::Void: {
        returnCleanup();
        break;
      }
      case ExprType::I32: {
        RegI32 rv = popI32(RegI32(ReturnReg));
        returnCleanup();
        freeI32(rv);
        break;
      }
      case ExprType::I64: {
        RegI64 rv = popI64(RegI64(ReturnReg64));
        returnCleanup();
        freeI64(rv);
        break;
      }
      case ExprType::F64: {
        RegF64 rv = popF64(RegF64(ReturnDoubleReg));
        returnCleanup();
        freeF64(rv);
        break;
      }
      case ExprType::F32: {
        RegF32 rv = popF32(RegF32(ReturnFloat32Reg));
        returnCleanup();
        freeF32(rv);
        break;
      }
      default: {
        MOZ_CRASH("Function return type");
      }
    }
}

bool
BaseCompiler::emitReturn()
{
    Nothing unused_value;
    if (!iter_.readReturn(&unused_value))
        return false;

    if (deadCode_)
        return true;

    doReturn(func_.sig().ret());
    deadCode_ = true;

    popValueStackTo(ctl_.back().stackSize);

    return true;
}

bool
BaseCompiler::emitCallArgs(const ValTypeVector& args, FunctionCall& baselineCall)
{
    MOZ_ASSERT(!deadCode_);

    startCallArgs(baselineCall, stackArgAreaSize(args));

    uint32_t numArgs = args.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        Nothing arg_;
        if (!iter_.readCallArg(argType, numArgs, i, &arg_))
            return false;
        Stk& arg = peek(numArgs - 1 - i);
        passArg(baselineCall, argType, arg);
    }

    // Pass the TLS pointer as a hidden argument in WasmTlsReg.  Load
    // it directly out if its stack slot so we don't interfere with
    // the stk_.
    if (baselineCall.loadTlsBefore)
        loadFromFramePtr(WasmTlsReg, frameOffsetFromSlot(tlsSlot_, MIRType::Pointer));

    if (!iter_.readCallArgsEnd(numArgs))
        return false;

    return true;
}

void
BaseCompiler::pushReturned(const FunctionCall& call, ExprType type)
{
    switch (type) {
      case ExprType::Void:
        MOZ_CRASH("Compiler bug: attempt to push void return");
        break;
      case ExprType::I32: {
        RegI32 rv = captureReturnedI32();
        pushI32(rv);
        break;
      }
      case ExprType::I64: {
        RegI64 rv = captureReturnedI64();
        pushI64(rv);
        break;
      }
      case ExprType::F32: {
        RegF32 rv = captureReturnedF32(call);
        pushF32(rv);
        break;
      }
      case ExprType::F64: {
        RegF64 rv = captureReturnedF64(call);
        pushF64(rv);
        break;
      }
      default:
        MOZ_CRASH("Function return type");
    }
}

// For now, always sync() at the beginning of the call to easily save
// live values.
//
// TODO / OPTIMIZE: We may be able to avoid a full sync(), since all
// we want is to save live registers that won't be saved by the callee
// or that we need for outgoing args - we don't need to sync the
// locals.  We can just push the necessary registers, it'll be like a
// lightweight sync.
//
// Even some of the pushing may be unnecessary if the registers
// will be consumed by the call, because then what we want is
// parallel assignment to the argument registers or onto the stack
// for outgoing arguments.  A sync() is just simpler.

bool
BaseCompiler::emitCallImportCommon(uint32_t lineOrBytecode, uint32_t funcImportIndex)
{
    const FuncImportGenDesc& funcImport = mg_.funcImports[funcImportIndex];
    const Sig& sig = *funcImport.sig;

    if (deadCode_)
        return true;

    sync();

    uint32_t numArgs = sig.args().length();
    size_t stackSpace = stackConsumed(numArgs);

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::Wasm, InterModule::True);

    if (!emitCallArgs(sig.args(), baselineCall))
        return false;

    if (!iter_.readCallReturn(sig.ret()))
        return false;

    callImport(funcImport.globalDataOffset, baselineCall);

    endCall(baselineCall);

    // TODO / OPTIMIZE: It would be better to merge this freeStack()
    // into the one in endCall, if we can.

    popValueStackBy(numArgs);
    masm.freeStack(stackSpace);

    if (!IsVoid(sig.ret()))
        pushReturned(baselineCall, sig.ret());

    return true;
}

bool
BaseCompiler::emitCall()
{
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    uint32_t calleeIndex;
    if (!iter_.readCall(&calleeIndex))
        return false;

    // For asm.js, imports are not part of the function index space so in
    // these cases firstFuncDefIndex is fixed to 0, even if there are
    // function imports.
    if (calleeIndex < mg_.firstFuncDefIndex)
        return emitCallImportCommon(lineOrBytecode, calleeIndex);

    if (deadCode_)
        return true;

    sync();

    uint32_t funcDefIndex = calleeIndex - mg_.firstFuncDefIndex;
    const Sig& sig = *mg_.funcDefSigs[funcDefIndex];
    uint32_t numArgs = sig.args().length();
    size_t stackSpace = stackConsumed(numArgs);

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::Wasm, InterModule::False);

    if (!emitCallArgs(sig.args(), baselineCall))
        return false;

    if (!iter_.readCallReturn(sig.ret()))
        return false;

    callDefinition(funcDefIndex, baselineCall);

    endCall(baselineCall);

    // TODO / OPTIMIZE: It would be better to merge this freeStack()
    // into the one in endCall, if we can.

    popValueStackBy(numArgs);
    masm.freeStack(stackSpace);

    if (!IsVoid(sig.ret()))
        pushReturned(baselineCall, sig.ret());

    return true;
}

bool
BaseCompiler::emitOldCallImport()
{
    MOZ_ASSERT(!mg_.firstFuncDefIndex);

    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    uint32_t funcImportIndex;
    if (!iter_.readCall(&funcImportIndex))
        return false;

    if (deadCode_)
        return true;

    return emitCallImportCommon(lineOrBytecode, funcImportIndex);
}

bool
BaseCompiler::emitCallIndirect(bool oldStyle)
{
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    uint32_t sigIndex;
    Nothing callee_;
    if (oldStyle) {
        if (!iter_.readOldCallIndirect(&sigIndex))
            return false;
    } else {
        if (!iter_.readCallIndirect(&sigIndex, &callee_))
            return false;
    }

    if (deadCode_)
        return true;

    sync();

    const SigWithId& sig = mg_.sigs[sigIndex];

    // new style: Stack: ... arg1 .. argn callee
    // old style: Stack: ... callee arg1 .. argn

    uint32_t numArgs = sig.args().length();
    size_t stackSpace = stackConsumed(numArgs + 1);

    // The arguments must be at the stack top for emitCallArgs, so pop the
    // callee if it is on top.  Note this only pops the compiler's stack,
    // not the CPU stack.

    Stk callee = oldStyle ? peek(numArgs) : stk_.popCopy();

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::Wasm, InterModule::True);

    if (!emitCallArgs(sig.args(), baselineCall))
        return false;

    if (oldStyle) {
        if (!iter_.readOldCallIndirectCallee(&callee_))
            return false;
    }

    if (!iter_.readCallReturn(sig.ret()))
        return false;

    callIndirect(sigIndex, callee, baselineCall);

    endCall(baselineCall);

    // For new style calls, the callee was popped off the compiler's
    // stack above.

    popValueStackBy(oldStyle ? numArgs + 1 : numArgs);

    // TODO / OPTIMIZE: It would be better to merge this freeStack()
    // into the one in endCall, if we can.

    masm.freeStack(stackSpace);

    if (!IsVoid(sig.ret()))
        pushReturned(baselineCall, sig.ret());

    return true;
}

bool
BaseCompiler::emitCommonMathCall(uint32_t lineOrBytecode, SymbolicAddress callee,
                                 ValTypeVector& signature, ExprType retType)
{
    sync();

    uint32_t numArgs = signature.length();
    size_t stackSpace = stackConsumed(numArgs);

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::System, InterModule::False);

    if (!emitCallArgs(signature, baselineCall))
        return false;

    if (!iter_.readCallReturn(retType))
      return false;

    builtinCall(callee, baselineCall);

    endCall(baselineCall);

    // TODO / OPTIMIZE: It would be better to merge this freeStack()
    // into the one in endCall, if we can.

    popValueStackBy(numArgs);
    masm.freeStack(stackSpace);

    pushReturned(baselineCall, retType);

    return true;
}

bool
BaseCompiler::emitUnaryMathBuiltinCall(SymbolicAddress callee, ValType operandType)
{
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    if (deadCode_)
        return true;

    return emitCommonMathCall(lineOrBytecode, callee,
                              operandType == ValType::F32 ? SigF_ : SigD_,
                              operandType == ValType::F32 ? ExprType::F32 : ExprType::F64);
}

bool
BaseCompiler::emitBinaryMathBuiltinCall(SymbolicAddress callee, ValType operandType)
{
    MOZ_ASSERT(operandType == ValType::F64);

    uint32_t lineOrBytecode = 0;
    if (callee == SymbolicAddress::ModD) {
        // Not actually a call in the binary representation
    } else {
        lineOrBytecode = readCallSiteLineOrBytecode();
    }

    if (deadCode_)
        return true;

    return emitCommonMathCall(lineOrBytecode, callee, SigDD_, ExprType::F64);
}

#ifdef QUOT_REM_I64_CALLOUT
bool
BaseCompiler::emitDivOrModI64BuiltinCall(SymbolicAddress callee, ValType operandType)
{
    MOZ_ASSERT(operandType == ValType::I64);

    if (deadCode_)
        return true;

    sync();

    needI64(abiReturnRegI64);

    RegI32 temp = needI32();
    RegI64 rhs = popI64();
    RegI64 srcDest = popI64ToSpecific(abiReturnRegI64);

    Label done;

    checkDivideByZeroI64(rhs, srcDest, &done);

    if (callee == SymbolicAddress::DivI64)
        checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(false));
    else if (callee == SymbolicAddress::ModI64)
        checkDivideSignedOverflowI64(rhs, srcDest, &done, ZeroOnOverflow(true));

    masm.setupUnalignedABICall(temp.reg);
    masm.passABIArg(srcDest.reg.high);
    masm.passABIArg(srcDest.reg.low);
    masm.passABIArg(rhs.reg.high);
    masm.passABIArg(rhs.reg.low);
    masm.callWithABI(callee);

    masm.bind(&done);

    freeI32(temp);
    freeI64(rhs);
    pushI64(srcDest);

    return true;
}
#endif

#ifdef I64_TO_FLOAT_CALLOUT
bool
BaseCompiler::emitConvertInt64ToFloatingCallout(SymbolicAddress callee, ValType operandType,
                                                ValType resultType)
{
    sync();

    RegI32 temp = needI32();
    RegI64 input = popI64();

    FunctionCall call(0);

    masm.setupUnalignedABICall(temp.reg);
# ifdef JS_NUNBOX32
    masm.passABIArg(input.reg.high);
    masm.passABIArg(input.reg.low);
# else
    MOZ_CRASH("BaseCompiler platform hook: emitConvertInt64ToFloatingCallout");
# endif
    masm.callWithABI(callee, MoveOp::DOUBLE);

    freeI32(temp);
    freeI64(input);

    RegF64 rv = captureReturnedF64(call);

    if (resultType == ValType::F32) {
        RegF32 rv2 = needF32();
        masm.convertDoubleToFloat32(rv.reg, rv2.reg);
        freeF64(rv);
        pushF32(rv2);
    } else {
        pushF64(rv);
    }

    return true;
}
#endif

#ifdef FLOAT_TO_I64_CALLOUT
// `Callee` always takes a double, so a float32 input must be converted.
bool
BaseCompiler::emitConvertFloatingToInt64Callout(SymbolicAddress callee, ValType operandType,
                                                ValType resultType)
{
    RegF64 doubleInput;
    if (operandType == ValType::F32) {
        doubleInput = needF64();
        RegF32 input = popF32();
        masm.convertFloat32ToDouble(input.reg, doubleInput.reg);
        freeF32(input);
    } else {
        doubleInput = popF64();
    }

    // We may need the value after the call for the ool check.
    RegF64 otherReg = needF64();
    moveF64(doubleInput, otherReg);
    pushF64(otherReg);

    sync();

    RegI32 temp = needI32();
    FunctionCall call(0);

    masm.setupUnalignedABICall(temp.reg);
    masm.passABIArg(doubleInput.reg, MoveOp::DOUBLE);
    masm.callWithABI(callee);

    freeI32(temp);
    freeF64(doubleInput);

    RegI64 rv = captureReturnedI64();

    RegF64 inputVal = popF64();

    bool isUnsigned = callee == SymbolicAddress::TruncateDoubleToUint64;

    // The OOL check just succeeds or fails, it does not generate a value.
    OutOfLineCode* ool = new (alloc_) OutOfLineTruncateCheckF32OrF64ToI64(AnyReg(inputVal),
                                                                          isUnsigned,
                                                                          trapOffset());
    ool = addOutOfLineCode(ool);
    if (!ool)
        return false;

    masm.branch64(Assembler::Equal, rv.reg, Imm64(0x8000000000000000), ool->entry());
    masm.bind(ool->rejoin());

    pushI64(rv);
    freeF64(inputVal);

    return true;
}
#endif

bool
BaseCompiler::emitGetLocal()
{
    uint32_t slot;
    if (!iter_.readGetLocal(locals_, &slot))
        return false;

    if (deadCode_)
        return true;

    // Local loads are pushed unresolved, ie, they may be deferred
    // until needed, until they may be affected by a store, or until a
    // sync.  This is intended to reduce register pressure.

    switch (locals_[slot]) {
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
      default:
        MOZ_CRASH("Local variable type");
    }

    return true;
}

bool
BaseCompiler::emitSetLocal()
{
    uint32_t slot;
    Nothing unused_value;
    if (!iter_.readSetLocal(locals_, &slot, &unused_value))
        return false;

    if (deadCode_)
        return true;

    switch (locals_[slot]) {
      case ValType::I32: {
        RegI32 rv = popI32();
        syncLocal(slot);
        storeToFrameI32(rv.reg, frameOffsetFromSlot(slot, MIRType::Int32));
        freeI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        syncLocal(slot);
        storeToFrameI64(rv.reg, frameOffsetFromSlot(slot, MIRType::Int64));
        freeI64(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        syncLocal(slot);
        storeToFrameF64(rv.reg, frameOffsetFromSlot(slot, MIRType::Double));
        freeF64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        syncLocal(slot);
        storeToFrameF32(rv.reg, frameOffsetFromSlot(slot, MIRType::Float32));
        freeF32(rv);
        break;
      }
      default:
        MOZ_CRASH("Local variable type");
    }

    return true;
}

bool
BaseCompiler::emitTeeLocal()
{
    uint32_t slot;
    Nothing unused_value;
    if (!iter_.readTeeLocal(locals_, &slot, &unused_value))
        return false;

    if (deadCode_)
        return true;

    switch (locals_[slot]) {
      case ValType::I32: {
        RegI32 rv = popI32();
        syncLocal(slot);
        storeToFrameI32(rv.reg, frameOffsetFromSlot(slot, MIRType::Int32));
        pushI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        syncLocal(slot);
        storeToFrameI64(rv.reg, frameOffsetFromSlot(slot, MIRType::Int64));
        pushI64(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        syncLocal(slot);
        storeToFrameF64(rv.reg, frameOffsetFromSlot(slot, MIRType::Double));
        pushF64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        syncLocal(slot);
        storeToFrameF32(rv.reg, frameOffsetFromSlot(slot, MIRType::Float32));
        pushF32(rv);
        break;
      }
      default:
        MOZ_CRASH("Local variable type");
    }

    return true;
}

bool
BaseCompiler::emitGetGlobal()
{
    uint32_t id;
    if (!iter_.readGetGlobal(mg_.globals, &id))
        return false;

    if (deadCode_)
        return true;

    const GlobalDesc& global = mg_.globals[id];

    if (global.isConstant()) {
        Val value = global.constantValue();
        switch (value.type()) {
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
          default:
            MOZ_CRASH("Global constant type");
        }
        return true;
    }

    switch (global.type()) {
      case ValType::I32: {
        RegI32 rv = needI32();
        loadGlobalVarI32(global.offset(), rv);
        pushI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = needI64();
        loadGlobalVarI64(global.offset(), rv);
        pushI64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = needF32();
        loadGlobalVarF32(global.offset(), rv);
        pushF32(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = needF64();
        loadGlobalVarF64(global.offset(), rv);
        pushF64(rv);
        break;
      }
      default:
        MOZ_CRASH("Global variable type");
        break;
    }
    return true;
}

bool
BaseCompiler::emitSetGlobal()
{
    uint32_t id;
    Nothing unused_value;
    if (!iter_.readSetGlobal(mg_.globals, &id, &unused_value))
        return false;

    if (deadCode_)
        return true;

    const GlobalDesc& global = mg_.globals[id];

    switch (global.type()) {
      case ValType::I32: {
        RegI32 rv = popI32();
        storeGlobalVarI32(global.offset(), rv);
        freeI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        storeGlobalVarI64(global.offset(), rv);
        freeI64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        storeGlobalVarF32(global.offset(), rv);
        freeF32(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        storeGlobalVarF64(global.offset(), rv);
        freeF64(rv);
        break;
      }
      default:
        MOZ_CRASH("Global variable type");
        break;
    }
    return true;
}

bool
BaseCompiler::emitTeeGlobal()
{
    uint32_t id;
    Nothing unused_value;
    if (!iter_.readTeeGlobal(mg_.globals, &id, &unused_value))
        return false;

    if (deadCode_)
        return true;

    const GlobalDesc& global = mg_.globals[id];

    switch (global.type()) {
      case ValType::I32: {
        RegI32 rv = popI32();
        storeGlobalVarI32(global.offset(), rv);
        pushI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        storeGlobalVarI64(global.offset(), rv);
        pushI64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        storeGlobalVarF32(global.offset(), rv);
        pushF32(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        storeGlobalVarF64(global.offset(), rv);
        pushF64(rv);
        break;
      }
      default:
        MOZ_CRASH("Global variable type");
        break;
    }
    return true;
}

bool
BaseCompiler::emitLoad(ValType type, Scalar::Type viewType)
{
    LinearMemoryAddress<Nothing> addr;
    if (!iter_.readLoad(type, Scalar::byteSize(viewType), &addr))
        return false;

    if (deadCode_)
        return true;

    // TODO / OPTIMIZE: Disable bounds checking on constant accesses
    // below the minimum heap length.

    MemoryAccessDesc access(viewType, addr.align, addr.offset, trapIfNotAsmJS());

    size_t temps = loadStoreTemps(access);
    RegI32 tmp1 = temps >= 1 ? needI32() : invalidI32();
    RegI32 tmp2 = temps >= 2 ? needI32() : invalidI32();

    switch (type) {
      case ValType::I32: {
        RegI32 rp = popI32();
#ifdef JS_CODEGEN_ARM
        RegI32 rv = access.isUnaligned() ? needI32() : rp;
#else
        RegI32 rv = rp;
#endif
        if (!load(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        pushI32(rv);
        if (rp != rv)
            freeI32(rp);
        break;
      }
      case ValType::I64: {
        RegI64 rv;
        RegI32 rp;
#ifdef JS_CODEGEN_X86
        rv = abiReturnRegI64;
        needI64(rv);
        rp = popI32();
#else
        rp = popI32();
        rv = needI64();
#endif
        if (!load(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        pushI64(rv);
        freeI32(rp);
        break;
      }
      case ValType::F32: {
        RegI32 rp = popI32();
        RegF32 rv = needF32();
        if (!load(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        pushF32(rv);
        freeI32(rp);
        break;
      }
      case ValType::F64: {
        RegI32 rp = popI32();
        RegF64 rv = needF64();
        if (!load(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        pushF64(rv);
        freeI32(rp);
        break;
      }
      default:
        MOZ_CRASH("load type");
        break;
    }

    if (temps >= 1)
        freeI32(tmp1);
    if (temps >= 2)
        freeI32(tmp2);

    return true;
}

bool
BaseCompiler::emitStore(ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<Nothing> addr;
    Nothing unused_value;
    if (!iter_.readStore(resultType, Scalar::byteSize(viewType), &addr, &unused_value))
        return false;

    if (deadCode_)
        return true;

    // TODO / OPTIMIZE: Disable bounds checking on constant accesses
    // below the minimum heap length.

    MemoryAccessDesc access(viewType, addr.align, addr.offset, trapIfNotAsmJS());

    size_t temps = loadStoreTemps(access);
    RegI32 tmp1 = temps >= 1 ? needI32() : invalidI32();
    RegI32 tmp2 = temps >= 2 ? needI32() : invalidI32();

    switch (resultType) {
      case ValType::I32: {
        RegI32 rp, rv;
        pop2xI32(&rp, &rv);
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        freeI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        freeI64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        freeF32(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        freeF64(rv);
        break;
      }
      default:
        MOZ_CRASH("store type");
        break;
    }

    if (temps >= 1)
        freeI32(tmp1);
    if (temps >= 2)
        freeI32(tmp2);

    return true;
}

bool
BaseCompiler::emitTeeStore(ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<Nothing> addr;
    Nothing unused_value;
    if (!iter_.readTeeStore(resultType, Scalar::byteSize(viewType), &addr, &unused_value))
        return false;

    if (deadCode_)
        return true;

    // TODO / OPTIMIZE: Disable bounds checking on constant accesses
    // below the minimum heap length.

    MemoryAccessDesc access(viewType, addr.align, addr.offset, trapIfNotAsmJS());

    size_t temps = loadStoreTemps(access);
    RegI32 tmp1 = temps >= 1 ? needI32() : invalidI32();
    RegI32 tmp2 = temps >= 2 ? needI32() : invalidI32();

    switch (resultType) {
      case ValType::I32: {
        RegI32 rp, rv;
        pop2xI32(&rp, &rv);
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        pushI32(rv);
        break;
      }
      case ValType::I64: {
        RegI64 rv = popI64();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        pushI64(rv);
        break;
      }
      case ValType::F32: {
        RegF32 rv = popF32();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        pushF32(rv);
        break;
      }
      case ValType::F64: {
        RegF64 rv = popF64();
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rv), tmp1, tmp2))
            return false;
        freeI32(rp);
        pushF64(rv);
        break;
      }
      default:
        MOZ_CRASH("store type");
        break;
    }

    if (temps >= 1)
        freeI32(tmp1);
    if (temps >= 2)
        freeI32(tmp2);

    return true;
}

bool
BaseCompiler::emitSelect()
{
    ValType type;
    Nothing unused_trueValue;
    Nothing unused_falseValue;
    Nothing unused_condition;
    if (!iter_.readSelect(&type, &unused_trueValue, &unused_falseValue, &unused_condition))
        return false;

    if (deadCode_)
        return true;

    // I32 condition on top, then false, then true.

    RegI32 rc = popI32();
    switch (type) {
      case ValType::I32: {
        Label done;
        RegI32 r0, r1;
        pop2xI32(&r0, &r1);
        masm.branch32(Assembler::NotEqual, rc.reg, Imm32(0), &done);
        moveI32(r1, r0);
        masm.bind(&done);
        freeI32(r1);
        pushI32(r0);
        break;
      }
      case ValType::I64: {
        Label done;
        RegI64 r0, r1;
        pop2xI64(&r0, &r1);
        masm.branch32(Assembler::NotEqual, rc.reg, Imm32(0), &done);
        moveI64(r1, r0);
        masm.bind(&done);
        freeI64(r1);
        pushI64(r0);
        break;
      }
      case ValType::F32: {
        Label done;
        RegF32 r0, r1;
        pop2xF32(&r0, &r1);
        masm.branch32(Assembler::NotEqual, rc.reg, Imm32(0), &done);
        moveF32(r1, r0);
        masm.bind(&done);
        freeF32(r1);
        pushF32(r0);
        break;
      }
      case ValType::F64: {
        Label done;
        RegF64 r0, r1;
        pop2xF64(&r0, &r1);
        masm.branch32(Assembler::NotEqual, rc.reg, Imm32(0), &done);
        moveF64(r1, r0);
        masm.bind(&done);
        freeF64(r1);
        pushF64(r0);
        break;
      }
      default: {
        MOZ_CRASH("select type");
      }
    }
    freeI32(rc);

    return true;
}

void
BaseCompiler::emitCompareI32(JSOp compareOp, MCompare::CompareType compareType)
{
    // TODO / OPTIMIZE: if we want to generate good code for boolean
    // operators for control it is possible to delay generating code
    // here by pushing a compare operation on the stack, after all it
    // is side-effect free.  The popping code for br_if will handle it
    // differently, but other popI32() will just force code generation.
    //
    // TODO / OPTIMIZE: Comparisons against constants using the same
    // popConstant pattern as for add().

    MOZ_ASSERT(compareType == MCompare::Compare_Int32 || compareType == MCompare::Compare_UInt32);
    RegI32 r0, r1;
    pop2xI32(&r0, &r1);
    bool u = compareType == MCompare::Compare_UInt32;
    switch (compareOp) {
      case JSOP_EQ:
        masm.cmp32Set(Assembler::Equal, r0.reg, r1.reg, r0.reg);
        break;
      case JSOP_NE:
        masm.cmp32Set(Assembler::NotEqual, r0.reg, r1.reg, r0.reg);
        break;
      case JSOP_LE:
        masm.cmp32Set(u ? Assembler::BelowOrEqual : Assembler::LessThanOrEqual, r0.reg, r1.reg, r0.reg);
        break;
      case JSOP_LT:
        masm.cmp32Set(u ? Assembler::Below : Assembler::LessThan, r0.reg, r1.reg, r0.reg);
        break;
      case JSOP_GE:
        masm.cmp32Set(u ? Assembler::AboveOrEqual : Assembler::GreaterThanOrEqual, r0.reg, r1.reg, r0.reg);
        break;
      case JSOP_GT:
        masm.cmp32Set(u ? Assembler::Above : Assembler::GreaterThan, r0.reg, r1.reg, r0.reg);
        break;
      default:
        MOZ_CRASH("Compiler bug: Unexpected compare opcode");
    }
    freeI32(r1);
    pushI32(r0);
}

void
BaseCompiler::emitCompareI64(JSOp compareOp, MCompare::CompareType compareType)
{
    MOZ_ASSERT(compareType == MCompare::Compare_Int64 || compareType == MCompare::Compare_UInt64);
    RegI64 r0, r1;
    pop2xI64(&r0, &r1);
    RegI32 i0(fromI64(r0));
    bool u = compareType == MCompare::Compare_UInt64;
    switch (compareOp) {
      case JSOP_EQ:
        cmp64Set(Assembler::Equal, r0, r1, i0);
        break;
      case JSOP_NE:
        cmp64Set(Assembler::NotEqual, r0, r1, i0);
        break;
      case JSOP_LE:
        cmp64Set(u ? Assembler::BelowOrEqual : Assembler::LessThanOrEqual, r0, r1, i0);
        break;
      case JSOP_LT:
        cmp64Set(u ? Assembler::Below : Assembler::LessThan, r0, r1, i0);
        break;
      case JSOP_GE:
        cmp64Set(u ? Assembler::AboveOrEqual : Assembler::GreaterThanOrEqual, r0, r1, i0);
        break;
      case JSOP_GT:
        cmp64Set(u ? Assembler::Above : Assembler::GreaterThan, r0, r1, i0);
        break;
      default:
        MOZ_CRASH("Compiler bug: Unexpected compare opcode");
    }
    freeI64(r1);
    freeI64Except(r0, i0);
    pushI32(i0);
}

void
BaseCompiler::emitCompareF32(JSOp compareOp, MCompare::CompareType compareType)
{
    MOZ_ASSERT(compareType == MCompare::Compare_Float32);
    Label across;
    RegF32 r0, r1;
    pop2xF32(&r0, &r1);
    RegI32 i0 = needI32();
    masm.mov(ImmWord(1), i0.reg);
    switch (compareOp) {
      case JSOP_EQ:
        masm.branchFloat(Assembler::DoubleEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_NE:
        masm.branchFloat(Assembler::DoubleNotEqualOrUnordered, r0.reg, r1.reg, &across);
        break;
      case JSOP_LE:
        masm.branchFloat(Assembler::DoubleLessThanOrEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_LT:
        masm.branchFloat(Assembler::DoubleLessThan, r0.reg, r1.reg, &across);
        break;
      case JSOP_GE:
        masm.branchFloat(Assembler::DoubleGreaterThanOrEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_GT:
        masm.branchFloat(Assembler::DoubleGreaterThan, r0.reg, r1.reg, &across);
        break;
      default:
        MOZ_CRASH("Compiler bug: Unexpected compare opcode");
    }
    masm.mov(ImmWord(0), i0.reg);
    masm.bind(&across);
    freeF32(r0);
    freeF32(r1);
    pushI32(i0);
}

void
BaseCompiler::emitCompareF64(JSOp compareOp, MCompare::CompareType compareType)
{
    MOZ_ASSERT(compareType == MCompare::Compare_Double);
    Label across;
    RegF64 r0, r1;
    pop2xF64(&r0, &r1);
    RegI32 i0 = needI32();
    masm.mov(ImmWord(1), i0.reg);
    switch (compareOp) {
      case JSOP_EQ:
        masm.branchDouble(Assembler::DoubleEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_NE:
        masm.branchDouble(Assembler::DoubleNotEqualOrUnordered, r0.reg, r1.reg, &across);
        break;
      case JSOP_LE:
        masm.branchDouble(Assembler::DoubleLessThanOrEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_LT:
        masm.branchDouble(Assembler::DoubleLessThan, r0.reg, r1.reg, &across);
        break;
      case JSOP_GE:
        masm.branchDouble(Assembler::DoubleGreaterThanOrEqual, r0.reg, r1.reg, &across);
        break;
      case JSOP_GT:
        masm.branchDouble(Assembler::DoubleGreaterThan, r0.reg, r1.reg, &across);
        break;
      default:
        MOZ_CRASH("Compiler bug: Unexpected compare opcode");
    }
    masm.mov(ImmWord(0), i0.reg);
    masm.bind(&across);
    freeF64(r0);
    freeF64(r1);
    pushI32(i0);
}

bool
BaseCompiler::emitTeeStoreWithCoercion(ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<Nothing> addr;
    Nothing unused_value;
    if (!iter_.readTeeStore(resultType, Scalar::byteSize(viewType), &addr, &unused_value))
        return false;

    if (deadCode_)
        return true;

    // TODO / OPTIMIZE: Disable bounds checking on constant accesses
    // below the minimum heap length.

    MemoryAccessDesc access(viewType, addr.align, addr.offset, trapIfNotAsmJS());

    size_t temps = loadStoreTemps(access);
    RegI32 tmp1 = temps >= 1 ? needI32() : invalidI32();
    RegI32 tmp2 = temps >= 2 ? needI32() : invalidI32();

    if (resultType == ValType::F32 && viewType == Scalar::Float64) {
        RegF32 rv = popF32();
        RegF64 rw = needF64();
        masm.convertFloat32ToDouble(rv.reg, rw.reg);
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rw), tmp1, tmp2))
            return false;
        pushF32(rv);
        freeI32(rp);
        freeF64(rw);
    }
    else if (resultType == ValType::F64 && viewType == Scalar::Float32) {
        RegF64 rv = popF64();
        RegF32 rw = needF32();
        masm.convertDoubleToFloat32(rv.reg, rw.reg);
        RegI32 rp = popI32();
        if (!store(access, rp, AnyReg(rw), tmp1, tmp2))
            return false;
        pushF64(rv);
        freeI32(rp);
        freeF32(rw);
    }
    else
        MOZ_CRASH("unexpected coerced store");

    if (temps >= 1)
        freeI32(tmp1);
    if (temps >= 2)
        freeI32(tmp2);

    return true;
}

bool
BaseCompiler::emitGrowMemory()
{
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    Nothing arg;
    if (!iter_.readGrowMemory(&arg))
        return false;

    if (deadCode_)
        return true;

    sync();

    uint32_t numArgs = 1;
    size_t stackSpace = stackConsumed(numArgs);

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::System, InterModule::True);

    ABIArg instanceArg = reservePointerArgument(baselineCall);

    startCallArgs(baselineCall, stackArgAreaSize(SigI_));
    passArg(baselineCall, ValType::I32, peek(0));
    builtinInstanceMethodCall(SymbolicAddress::GrowMemory, instanceArg, baselineCall);
    endCall(baselineCall);

    popValueStackBy(numArgs);
    masm.freeStack(stackSpace);

    pushReturned(baselineCall, ExprType::I32);

    return true;
}

bool
BaseCompiler::emitCurrentMemory()
{
    uint32_t lineOrBytecode = readCallSiteLineOrBytecode();

    if (!iter_.readCurrentMemory())
        return false;

    if (deadCode_)
        return true;

    sync();

    FunctionCall baselineCall(lineOrBytecode);
    beginCall(baselineCall, UseABI::System, InterModule::False);

    ABIArg instanceArg = reservePointerArgument(baselineCall);

    startCallArgs(baselineCall, stackArgAreaSize(Sig_));
    builtinInstanceMethodCall(SymbolicAddress::CurrentMemory, instanceArg, baselineCall);
    endCall(baselineCall);

    pushReturned(baselineCall, ExprType::I32);

    return true;
}

bool
BaseCompiler::emitBody()
{
    uint32_t overhead = 0;

    for (;;) {

        Nothing unused_a, unused_b;

#define emitBinary(doEmit, type) \
        iter_.readBinary(type, &unused_a, &unused_b) && (deadCode_ || (doEmit(), true))

#define emitUnary(doEmit, type) \
        iter_.readUnary(type, &unused_a) && (deadCode_ || (doEmit(), true))

#define emitComparison(doEmit, operandType, compareOp, compareType) \
        iter_.readComparison(operandType, &unused_a, &unused_b) && \
            (deadCode_ || (doEmit(compareOp, compareType), true))

#define emitConversion(doEmit, inType, outType) \
        iter_.readConversion(inType, outType, &unused_a) && (deadCode_ || (doEmit(), true))

#define emitConversionOOM(doEmit, inType, outType) \
        iter_.readConversion(inType, outType, &unused_a) && (deadCode_ || doEmit())

#define emitCalloutConversionOOM(doEmit, symbol, inType, outType) \
        iter_.readConversion(inType, outType, &unused_a) && \
            (deadCode_ || doEmit(symbol, inType, outType))

#define CHECK(E)      if (!(E)) goto done
#define NEXT()        continue
#define CHECK_NEXT(E) if (!(E)) goto done; continue

        // TODO / EVALUATE: Not obvious that this attempt at reducing
        // overhead is really paying off relative to making the check
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

        if (done())
            return true;

        Expr expr;
        CHECK(iter_.readExpr(&expr));

        switch (expr) {
          // Control opcodes
          case Expr::Nop:
            CHECK(iter_.readNop());
            NEXT();
          case Expr::Drop:
            CHECK(iter_.readDrop());
            if (!deadCode_)
                popValueStackBy(1);
            NEXT();
          case Expr::Block:
            CHECK_NEXT(emitBlock());
          case Expr::Loop:
            CHECK_NEXT(emitLoop());
          case Expr::If:
            CHECK_NEXT(emitIf());
          case Expr::Else:
            CHECK_NEXT(emitElse());
          case Expr::End:
            CHECK_NEXT(emitEnd());
          case Expr::Br:
            CHECK_NEXT(emitBr());
          case Expr::BrIf:
            CHECK_NEXT(emitBrIf());
          case Expr::BrTable:
            CHECK_NEXT(emitBrTable());
          case Expr::Return:
            CHECK_NEXT(emitReturn());
          case Expr::Unreachable:
            CHECK(iter_.readUnreachable());
            if (!deadCode_) {
                unreachableTrap();
                deadCode_ = true;
                popValueStackTo(ctl_.back().stackSize);
            }
            NEXT();

          // Calls
          case Expr::Call:
            CHECK_NEXT(emitCall());
          case Expr::CallIndirect:
            CHECK_NEXT(emitCallIndirect(/* oldStyle = */ false));
          case Expr::OldCallIndirect:
            CHECK_NEXT(emitCallIndirect(/* oldStyle = */ true));
          case Expr::OldCallImport:
            CHECK_NEXT(emitOldCallImport());

          // Locals and globals
          case Expr::GetLocal:
            CHECK_NEXT(emitGetLocal());
          case Expr::SetLocal:
            CHECK_NEXT(emitSetLocal());
          case Expr::TeeLocal:
            CHECK_NEXT(emitTeeLocal());
          case Expr::GetGlobal:
            CHECK_NEXT(emitGetGlobal());
          case Expr::SetGlobal:
            CHECK_NEXT(emitSetGlobal());
          case Expr::TeeGlobal:
            CHECK_NEXT(emitTeeGlobal());

          // Select
          case Expr::Select:
            CHECK_NEXT(emitSelect());

          // I32
          case Expr::I32Const: {
            int32_t i32;
            CHECK(iter_.readI32Const(&i32));
            if (!deadCode_)
                pushI32(i32);
            NEXT();
          }
          case Expr::I32Add:
            CHECK_NEXT(emitBinary(emitAddI32, ValType::I32));
          case Expr::I32Sub:
            CHECK_NEXT(emitBinary(emitSubtractI32, ValType::I32));
          case Expr::I32Mul:
            CHECK_NEXT(emitBinary(emitMultiplyI32, ValType::I32));
          case Expr::I32DivS:
            CHECK_NEXT(emitBinary(emitQuotientI32, ValType::I32));
          case Expr::I32DivU:
            CHECK_NEXT(emitBinary(emitQuotientU32, ValType::I32));
          case Expr::I32RemS:
            CHECK_NEXT(emitBinary(emitRemainderI32, ValType::I32));
          case Expr::I32RemU:
            CHECK_NEXT(emitBinary(emitRemainderU32, ValType::I32));
          case Expr::I32Min:
            CHECK_NEXT(emitBinary(emitMinI32, ValType::I32));
          case Expr::I32Max:
            CHECK_NEXT(emitBinary(emitMaxI32, ValType::I32));
          case Expr::I32Eqz:
            CHECK_NEXT(emitConversion(emitEqzI32, ValType::I32, ValType::I32));
          case Expr::I32TruncSF32:
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI32<false>, ValType::F32, ValType::I32));
          case Expr::I32TruncUF32:
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI32<true>, ValType::F32, ValType::I32));
          case Expr::I32TruncSF64:
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI32<false>, ValType::F64, ValType::I32));
          case Expr::I32TruncUF64:
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI32<true>, ValType::F64, ValType::I32));
          case Expr::I32WrapI64:
            CHECK_NEXT(emitConversion(emitWrapI64ToI32, ValType::I64, ValType::I32));
          case Expr::I32ReinterpretF32:
            CHECK_NEXT(emitConversion(emitReinterpretF32AsI32, ValType::F32, ValType::I32));
          case Expr::I32Clz:
            CHECK_NEXT(emitUnary(emitClzI32, ValType::I32));
          case Expr::I32Ctz:
            CHECK_NEXT(emitUnary(emitCtzI32, ValType::I32));
          case Expr::I32Popcnt:
            CHECK_NEXT(emitUnary(emitPopcntI32, ValType::I32));
          case Expr::I32Abs:
            CHECK_NEXT(emitUnary(emitAbsI32, ValType::I32));
          case Expr::I32Neg:
            CHECK_NEXT(emitUnary(emitNegateI32, ValType::I32));
          case Expr::I32Or:
            CHECK_NEXT(emitBinary(emitOrI32, ValType::I32));
          case Expr::I32And:
            CHECK_NEXT(emitBinary(emitAndI32, ValType::I32));
          case Expr::I32Xor:
            CHECK_NEXT(emitBinary(emitXorI32, ValType::I32));
          case Expr::I32Shl:
            CHECK_NEXT(emitBinary(emitShlI32, ValType::I32));
          case Expr::I32ShrS:
            CHECK_NEXT(emitBinary(emitShrI32, ValType::I32));
          case Expr::I32ShrU:
            CHECK_NEXT(emitBinary(emitShrU32, ValType::I32));
          case Expr::I32BitNot:
            CHECK_NEXT(emitUnary(emitBitNotI32, ValType::I32));
          case Expr::I32Load8S:
            CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int8));
          case Expr::I32Load8U:
            CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint8));
          case Expr::I32Load16S:
            CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int16));
          case Expr::I32Load16U:
            CHECK_NEXT(emitLoad(ValType::I32, Scalar::Uint16));
          case Expr::I32Load:
            CHECK_NEXT(emitLoad(ValType::I32, Scalar::Int32));
          case Expr::I32Store8:
            CHECK_NEXT(emitStore(ValType::I32, Scalar::Int8));
          case Expr::I32TeeStore8:
            CHECK_NEXT(emitTeeStore(ValType::I32, Scalar::Int8));
          case Expr::I32Store16:
            CHECK_NEXT(emitStore(ValType::I32, Scalar::Int16));
          case Expr::I32TeeStore16:
            CHECK_NEXT(emitTeeStore(ValType::I32, Scalar::Int16));
          case Expr::I32Store:
            CHECK_NEXT(emitStore(ValType::I32, Scalar::Int32));
          case Expr::I32TeeStore:
            CHECK_NEXT(emitTeeStore(ValType::I32, Scalar::Int32));
          case Expr::I32Rotr:
            CHECK_NEXT(emitBinary(emitRotrI32, ValType::I32));
          case Expr::I32Rotl:
            CHECK_NEXT(emitBinary(emitRotlI32, ValType::I32));

          // I64
          case Expr::I64Const: {
            int64_t i64;
            CHECK(iter_.readI64Const(&i64));
            if (!deadCode_)
                pushI64(i64);
            NEXT();
          }
          case Expr::I64Add:
            CHECK_NEXT(emitBinary(emitAddI64, ValType::I64));
          case Expr::I64Sub:
            CHECK_NEXT(emitBinary(emitSubtractI64, ValType::I64));
          case Expr::I64Mul:
            CHECK_NEXT(emitBinary(emitMultiplyI64, ValType::I64));
          case Expr::I64DivS:
#ifdef QUOT_REM_I64_CALLOUT
            CHECK_NEXT(emitDivOrModI64BuiltinCall(SymbolicAddress::DivI64, ValType::I64));
#else
            CHECK_NEXT(emitBinary(emitQuotientI64, ValType::I64));
#endif
          case Expr::I64DivU:
#ifdef QUOT_REM_I64_CALLOUT
            CHECK_NEXT(emitDivOrModI64BuiltinCall(SymbolicAddress::UDivI64, ValType::I64));
#else
            CHECK_NEXT(emitBinary(emitQuotientU64, ValType::I64));
#endif
          case Expr::I64RemS:
#ifdef QUOT_REM_I64_CALLOUT
            CHECK_NEXT(emitDivOrModI64BuiltinCall(SymbolicAddress::ModI64, ValType::I64));
#else
            CHECK_NEXT(emitBinary(emitRemainderI64, ValType::I64));
#endif
          case Expr::I64RemU:
#ifdef QUOT_REM_I64_CALLOUT
            CHECK_NEXT(emitDivOrModI64BuiltinCall(SymbolicAddress::UModI64, ValType::I64));
#else
            CHECK_NEXT(emitBinary(emitRemainderU64, ValType::I64));
#endif
          case Expr::I64TruncSF32:
#ifdef FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                                SymbolicAddress::TruncateDoubleToInt64,
                                                ValType::F32, ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI64<false>, ValType::F32, ValType::I64));
#endif
          case Expr::I64TruncUF32:
#ifdef FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                                SymbolicAddress::TruncateDoubleToUint64,
                                                ValType::F32, ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF32ToI64<true>, ValType::F32, ValType::I64));
#endif
          case Expr::I64TruncSF64:
#ifdef FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                                SymbolicAddress::TruncateDoubleToInt64,
                                                ValType::F64, ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI64<false>, ValType::F64, ValType::I64));
#endif
          case Expr::I64TruncUF64:
#ifdef FLOAT_TO_I64_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertFloatingToInt64Callout,
                                                SymbolicAddress::TruncateDoubleToUint64,
                                                ValType::F64, ValType::I64));
#else
            CHECK_NEXT(emitConversionOOM(emitTruncateF64ToI64<true>, ValType::F64, ValType::I64));
#endif
          case Expr::I64ExtendSI32:
            CHECK_NEXT(emitConversion(emitExtendI32ToI64, ValType::I32, ValType::I64));
          case Expr::I64ExtendUI32:
            CHECK_NEXT(emitConversion(emitExtendU32ToI64, ValType::I32, ValType::I64));
          case Expr::I64ReinterpretF64:
            CHECK_NEXT(emitConversion(emitReinterpretF64AsI64, ValType::F64, ValType::I64));
          case Expr::I64Or:
            CHECK_NEXT(emitBinary(emitOrI64, ValType::I64));
          case Expr::I64And:
            CHECK_NEXT(emitBinary(emitAndI64, ValType::I64));
          case Expr::I64Xor:
            CHECK_NEXT(emitBinary(emitXorI64, ValType::I64));
          case Expr::I64Shl:
            CHECK_NEXT(emitBinary(emitShlI64, ValType::I64));
          case Expr::I64ShrS:
            CHECK_NEXT(emitBinary(emitShrI64, ValType::I64));
          case Expr::I64ShrU:
            CHECK_NEXT(emitBinary(emitShrU64, ValType::I64));
          case Expr::I64Rotr:
            CHECK_NEXT(emitBinary(emitRotrI64, ValType::I64));
          case Expr::I64Rotl:
            CHECK_NEXT(emitBinary(emitRotlI64, ValType::I64));
          case Expr::I64Clz:
            CHECK_NEXT(emitUnary(emitClzI64, ValType::I64));
          case Expr::I64Ctz:
            CHECK_NEXT(emitUnary(emitCtzI64, ValType::I64));
          case Expr::I64Popcnt:
            CHECK_NEXT(emitUnary(emitPopcntI64, ValType::I64));
          case Expr::I64Eqz:
            CHECK_NEXT(emitConversion(emitEqzI64, ValType::I64, ValType::I32));
          case Expr::I64Load8S:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int8));
          case Expr::I64Load16S:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int16));
          case Expr::I64Load32S:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int32));
          case Expr::I64Load8U:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint8));
          case Expr::I64Load16U:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint16));
          case Expr::I64Load32U:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Uint32));
          case Expr::I64Load:
            CHECK_NEXT(emitLoad(ValType::I64, Scalar::Int64));
          case Expr::I64Store8:
            CHECK_NEXT(emitStore(ValType::I64, Scalar::Int8));
          case Expr::I64TeeStore8:
            CHECK_NEXT(emitTeeStore(ValType::I64, Scalar::Int8));
          case Expr::I64Store16:
            CHECK_NEXT(emitStore(ValType::I64, Scalar::Int16));
          case Expr::I64TeeStore16:
            CHECK_NEXT(emitTeeStore(ValType::I64, Scalar::Int16));
          case Expr::I64Store32:
            CHECK_NEXT(emitStore(ValType::I64, Scalar::Int32));
          case Expr::I64TeeStore32:
            CHECK_NEXT(emitTeeStore(ValType::I64, Scalar::Int32));
          case Expr::I64Store:
            CHECK_NEXT(emitStore(ValType::I64, Scalar::Int64));
          case Expr::I64TeeStore:
            CHECK_NEXT(emitTeeStore(ValType::I64, Scalar::Int64));

          // F32
          case Expr::F32Const: {
            RawF32 f32;
            CHECK(iter_.readF32Const(&f32));
            if (!deadCode_)
                pushF32(f32);
            NEXT();
          }
          case Expr::F32Add:
            CHECK_NEXT(emitBinary(emitAddF32, ValType::F32));
          case Expr::F32Sub:
            CHECK_NEXT(emitBinary(emitSubtractF32, ValType::F32));
          case Expr::F32Mul:
            CHECK_NEXT(emitBinary(emitMultiplyF32, ValType::F32));
          case Expr::F32Div:
            CHECK_NEXT(emitBinary(emitDivideF32, ValType::F32));
          case Expr::F32Min:
            CHECK_NEXT(emitBinary(emitMinF32, ValType::F32));
          case Expr::F32Max:
            CHECK_NEXT(emitBinary(emitMaxF32, ValType::F32));
          case Expr::F32Neg:
            CHECK_NEXT(emitUnary(emitNegateF32, ValType::F32));
          case Expr::F32Abs:
            CHECK_NEXT(emitUnary(emitAbsF32, ValType::F32));
          case Expr::F32Sqrt:
            CHECK_NEXT(emitUnary(emitSqrtF32, ValType::F32));
          case Expr::F32Ceil:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::CeilF, ValType::F32));
          case Expr::F32Floor:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::FloorF, ValType::F32));
          case Expr::F32DemoteF64:
            CHECK_NEXT(emitConversion(emitConvertF64ToF32, ValType::F64, ValType::F32));
          case Expr::F32ConvertSI32:
            CHECK_NEXT(emitConversion(emitConvertI32ToF32, ValType::I32, ValType::F32));
          case Expr::F32ConvertUI32:
            CHECK_NEXT(emitConversion(emitConvertU32ToF32, ValType::I32, ValType::F32));
          case Expr::F32ConvertSI64:
#ifdef I64_TO_FLOAT_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                                SymbolicAddress::Int64ToFloatingPoint,
                                                ValType::I64, ValType::F32));
#else
            CHECK_NEXT(emitConversion(emitConvertI64ToF32, ValType::I64, ValType::F32));
#endif
          case Expr::F32ConvertUI64:
#ifdef I64_TO_FLOAT_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                                SymbolicAddress::Uint64ToFloatingPoint,
                                                ValType::I64, ValType::F32));
#else
            CHECK_NEXT(emitConversion(emitConvertU64ToF32, ValType::I64, ValType::F32));
#endif
          case Expr::F32ReinterpretI32:
            CHECK_NEXT(emitConversion(emitReinterpretI32AsF32, ValType::I32, ValType::F32));
          case Expr::F32Load:
            CHECK_NEXT(emitLoad(ValType::F32, Scalar::Float32));
          case Expr::F32Store:
            CHECK_NEXT(emitStore(ValType::F32, Scalar::Float32));
          case Expr::F32TeeStore:
            CHECK_NEXT(emitTeeStore(ValType::F32, Scalar::Float32));
          case Expr::F32TeeStoreF64:
            CHECK_NEXT(emitTeeStoreWithCoercion(ValType::F32, Scalar::Float64));
          case Expr::F32CopySign:
            CHECK_NEXT(emitBinary(emitCopysignF32, ValType::F32));
          case Expr::F32Nearest:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntF, ValType::F32));
          case Expr::F32Trunc:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::TruncF, ValType::F32));

          // F64
          case Expr::F64Const: {
            RawF64 f64;
            CHECK(iter_.readF64Const(&f64));
            if (!deadCode_)
                pushF64(f64);
            NEXT();
          }
          case Expr::F64Add:
            CHECK_NEXT(emitBinary(emitAddF64, ValType::F64));
          case Expr::F64Sub:
            CHECK_NEXT(emitBinary(emitSubtractF64, ValType::F64));
          case Expr::F64Mul:
            CHECK_NEXT(emitBinary(emitMultiplyF64, ValType::F64));
          case Expr::F64Div:
            CHECK_NEXT(emitBinary(emitDivideF64, ValType::F64));
          case Expr::F64Mod:
            CHECK_NEXT(emitBinaryMathBuiltinCall(SymbolicAddress::ModD, ValType::F64));
          case Expr::F64Min:
            CHECK_NEXT(emitBinary(emitMinF64, ValType::F64));
          case Expr::F64Max:
            CHECK_NEXT(emitBinary(emitMaxF64, ValType::F64));
          case Expr::F64Neg:
            CHECK_NEXT(emitUnary(emitNegateF64, ValType::F64));
          case Expr::F64Abs:
            CHECK_NEXT(emitUnary(emitAbsF64, ValType::F64));
          case Expr::F64Sqrt:
            CHECK_NEXT(emitUnary(emitSqrtF64, ValType::F64));
          case Expr::F64Ceil:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::CeilD, ValType::F64));
          case Expr::F64Floor:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::FloorD, ValType::F64));
          case Expr::F64Sin:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::SinD, ValType::F64));
          case Expr::F64Cos:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::CosD, ValType::F64));
          case Expr::F64Tan:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::TanD, ValType::F64));
          case Expr::F64Asin:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::ASinD, ValType::F64));
          case Expr::F64Acos:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::ACosD, ValType::F64));
          case Expr::F64Atan:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::ATanD, ValType::F64));
          case Expr::F64Exp:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::ExpD, ValType::F64));
          case Expr::F64Log:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::LogD, ValType::F64));
          case Expr::F64Pow:
            CHECK_NEXT(emitBinaryMathBuiltinCall(SymbolicAddress::PowD, ValType::F64));
          case Expr::F64Atan2:
            CHECK_NEXT(emitBinaryMathBuiltinCall(SymbolicAddress::ATan2D, ValType::F64));
          case Expr::F64PromoteF32:
            CHECK_NEXT(emitConversion(emitConvertF32ToF64, ValType::F32, ValType::F64));
          case Expr::F64ConvertSI32:
            CHECK_NEXT(emitConversion(emitConvertI32ToF64, ValType::I32, ValType::F64));
          case Expr::F64ConvertUI32:
            CHECK_NEXT(emitConversion(emitConvertU32ToF64, ValType::I32, ValType::F64));
          case Expr::F64ConvertSI64:
#ifdef I64_TO_FLOAT_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                                SymbolicAddress::Int64ToFloatingPoint,
                                                ValType::I64, ValType::F64));
#else
            CHECK_NEXT(emitConversion(emitConvertI64ToF64, ValType::I64, ValType::F64));
#endif
          case Expr::F64ConvertUI64:
#ifdef I64_TO_FLOAT_CALLOUT
            CHECK_NEXT(emitCalloutConversionOOM(emitConvertInt64ToFloatingCallout,
                                                SymbolicAddress::Uint64ToFloatingPoint,
                                                ValType::I64, ValType::F64));
#else
            CHECK_NEXT(emitConversion(emitConvertU64ToF64, ValType::I64, ValType::F64));
#endif
          case Expr::F64Load:
            CHECK_NEXT(emitLoad(ValType::F64, Scalar::Float64));
          case Expr::F64Store:
            CHECK_NEXT(emitStore(ValType::F64, Scalar::Float64));
          case Expr::F64TeeStore:
            CHECK_NEXT(emitTeeStore(ValType::F64, Scalar::Float64));
          case Expr::F64TeeStoreF32:
            CHECK_NEXT(emitTeeStoreWithCoercion(ValType::F64, Scalar::Float32));
          case Expr::F64ReinterpretI64:
            CHECK_NEXT(emitConversion(emitReinterpretI64AsF64, ValType::I64, ValType::F64));
          case Expr::F64CopySign:
            CHECK_NEXT(emitBinary(emitCopysignF64, ValType::F64));
          case Expr::F64Nearest:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::NearbyIntD, ValType::F64));
          case Expr::F64Trunc:
            CHECK_NEXT(emitUnaryMathBuiltinCall(SymbolicAddress::TruncD, ValType::F64));

          // Comparisons
          case Expr::I32Eq:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_EQ, MCompare::Compare_Int32));
          case Expr::I32Ne:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_NE, MCompare::Compare_Int32));
          case Expr::I32LtS:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_LT, MCompare::Compare_Int32));
          case Expr::I32LeS:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_LE, MCompare::Compare_Int32));
          case Expr::I32GtS:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_GT, MCompare::Compare_Int32));
          case Expr::I32GeS:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_GE, MCompare::Compare_Int32));
          case Expr::I32LtU:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_LT, MCompare::Compare_UInt32));
          case Expr::I32LeU:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_LE, MCompare::Compare_UInt32));
          case Expr::I32GtU:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_GT, MCompare::Compare_UInt32));
          case Expr::I32GeU:
            CHECK_NEXT(emitComparison(emitCompareI32, ValType::I32, JSOP_GE, MCompare::Compare_UInt32));
          case Expr::I64Eq:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_EQ, MCompare::Compare_Int64));
          case Expr::I64Ne:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_NE, MCompare::Compare_Int64));
          case Expr::I64LtS:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_LT, MCompare::Compare_Int64));
          case Expr::I64LeS:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_LE, MCompare::Compare_Int64));
          case Expr::I64GtS:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_GT, MCompare::Compare_Int64));
          case Expr::I64GeS:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_GE, MCompare::Compare_Int64));
          case Expr::I64LtU:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_LT, MCompare::Compare_UInt64));
          case Expr::I64LeU:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_LE, MCompare::Compare_UInt64));
          case Expr::I64GtU:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_GT, MCompare::Compare_UInt64));
          case Expr::I64GeU:
            CHECK_NEXT(emitComparison(emitCompareI64, ValType::I64, JSOP_GE, MCompare::Compare_UInt64));
          case Expr::F32Eq:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_EQ, MCompare::Compare_Float32));
          case Expr::F32Ne:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_NE, MCompare::Compare_Float32));
          case Expr::F32Lt:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_LT, MCompare::Compare_Float32));
          case Expr::F32Le:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_LE, MCompare::Compare_Float32));
          case Expr::F32Gt:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_GT, MCompare::Compare_Float32));
          case Expr::F32Ge:
            CHECK_NEXT(emitComparison(emitCompareF32, ValType::F32, JSOP_GE, MCompare::Compare_Float32));
          case Expr::F64Eq:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_EQ, MCompare::Compare_Double));
          case Expr::F64Ne:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_NE, MCompare::Compare_Double));
          case Expr::F64Lt:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_LT, MCompare::Compare_Double));
          case Expr::F64Le:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_LE, MCompare::Compare_Double));
          case Expr::F64Gt:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_GT, MCompare::Compare_Double));
          case Expr::F64Ge:
            CHECK_NEXT(emitComparison(emitCompareF64, ValType::F64, JSOP_GE, MCompare::Compare_Double));

          // SIMD
#define CASE(TYPE, OP, SIGN) \
          case Expr::TYPE##OP: \
            MOZ_CRASH("Unimplemented SIMD");
#define I8x16CASE(OP) CASE(I8x16, OP, SimdSign::Signed)
#define I16x8CASE(OP) CASE(I16x8, OP, SimdSign::Signed)
#define I32x4CASE(OP) CASE(I32x4, OP, SimdSign::Signed)
#define F32x4CASE(OP) CASE(F32x4, OP, SimdSign::NotApplicable)
#define B8x16CASE(OP) CASE(B8x16, OP, SimdSign::NotApplicable)
#define B16x8CASE(OP) CASE(B16x8, OP, SimdSign::NotApplicable)
#define B32x4CASE(OP) CASE(B32x4, OP, SimdSign::NotApplicable)
#define ENUMERATE(TYPE, FORALL, DO) \
          case Expr::TYPE##Constructor: \
            FORALL(DO)

          ENUMERATE(I8x16, FORALL_INT8X16_ASMJS_OP, I8x16CASE)
          ENUMERATE(I16x8, FORALL_INT16X8_ASMJS_OP, I16x8CASE)
          ENUMERATE(I32x4, FORALL_INT32X4_ASMJS_OP, I32x4CASE)
          ENUMERATE(F32x4, FORALL_FLOAT32X4_ASMJS_OP, F32x4CASE)
          ENUMERATE(B8x16, FORALL_BOOL_SIMD_OP, B8x16CASE)
          ENUMERATE(B16x8, FORALL_BOOL_SIMD_OP, B16x8CASE)
          ENUMERATE(B32x4, FORALL_BOOL_SIMD_OP, B32x4CASE)

#undef CASE
#undef I8x16CASE
#undef I16x8CASE
#undef I32x4CASE
#undef F32x4CASE
#undef B8x16CASE
#undef B16x8CASE
#undef B32x4CASE
#undef ENUMERATE

          case Expr::I8x16Const:
          case Expr::I16x8Const:
          case Expr::I32x4Const:
          case Expr::F32x4Const:
          case Expr::B8x16Const:
          case Expr::B16x8Const:
          case Expr::B32x4Const:
          case Expr::I32x4shiftRightByScalarU:
          case Expr::I8x16addSaturateU:
          case Expr::I8x16subSaturateU:
          case Expr::I8x16shiftRightByScalarU:
          case Expr::I8x16lessThanU:
          case Expr::I8x16lessThanOrEqualU:
          case Expr::I8x16greaterThanU:
          case Expr::I8x16greaterThanOrEqualU:
          case Expr::I8x16extractLaneU:
          case Expr::I16x8addSaturateU:
          case Expr::I16x8subSaturateU:
          case Expr::I16x8shiftRightByScalarU:
          case Expr::I16x8lessThanU:
          case Expr::I16x8lessThanOrEqualU:
          case Expr::I16x8greaterThanU:
          case Expr::I16x8greaterThanOrEqualU:
          case Expr::I16x8extractLaneU:
          case Expr::I32x4lessThanU:
          case Expr::I32x4lessThanOrEqualU:
          case Expr::I32x4greaterThanU:
          case Expr::I32x4greaterThanOrEqualU:
          case Expr::I32x4fromFloat32x4U:
            MOZ_CRASH("Unimplemented SIMD");

          // Atomics
          case Expr::I32AtomicsLoad:
          case Expr::I32AtomicsStore:
          case Expr::I32AtomicsBinOp:
          case Expr::I32AtomicsCompareExchange:
          case Expr::I32AtomicsExchange:
            MOZ_CRASH("Unimplemented Atomics");

          // Memory Related
          case Expr::GrowMemory:
            CHECK_NEXT(emitGrowMemory());
          case Expr::CurrentMemory:
            CHECK_NEXT(emitCurrentMemory());

          case Expr::Limit:;
        }

        MOZ_CRASH("unexpected wasm opcode");

#undef CHECK
#undef NEXT
#undef CHECK_NEXT
#undef emitBinary
#undef emitUnary
#undef emitComparison
#undef emitConversion
#undef emitConversionOOM
#undef emitCalloutConversionOOM
    }

done:
    return false;
}

bool
BaseCompiler::emitFunction()
{
    // emitBody() will ensure that there is enough memory reserved in the
    // vector for infallible allocation to succeed within the compiler, but we
    // need a little headroom for the initial pushControl(), which pushes a
    // void value onto the value stack.

    if (!stk_.reserve(8))
        return false;

    const Sig& sig = func_.sig();

    if (!iter_.readFunctionStart(sig.ret()))
        return false;

    beginFunction();

    UniquePooledLabel functionEnd(newLabel());
    if (!pushControl(&functionEnd))
        return false;

#ifdef JS_CODEGEN_ARM64
    // FIXME: There is a hack up at the top to allow the baseline
    // compiler to compile on ARM64 (by defining StackPointer), but
    // the resulting code cannot run.  So prevent it from running.
    MOZ_CRASH("Several adjustments required for ARM64 operation");
#endif

    if (!emitBody())
        return false;

    if (!iter_.readFunctionEnd())
        return false;

    if (!endFunction())
        return false;

    return true;
}

BaseCompiler::BaseCompiler(const ModuleGeneratorData& mg,
                           Decoder& decoder,
                           const FuncBytes& func,
                           const ValTypeVector& locals,
                           FuncCompileResults& compileResults)
    : mg_(mg),
      iter_(decoder, func.lineOrBytecode()),
      func_(func),
      lastReadCallSite_(0),
      alloc_(compileResults.alloc()),
      locals_(locals),
      localSize_(0),
      varLow_(0),
      varHigh_(0),
      maxFramePushed_(0),
      deadCode_(false),
      prologueTrapOffset_(trapOffset()),
      compileResults_(compileResults),
      masm(compileResults_.masm()),
      availGPR_(GeneralRegisterSet::All()),
      availFPU_(FloatRegisterSet::All()),
#ifdef DEBUG
      scratchRegisterTaken_(false),
#endif
      tlsSlot_(0),
#ifdef JS_CODEGEN_X64
      specific_rax(RegI64(Register64(rax))),
      specific_rcx(RegI64(Register64(rcx))),
      specific_rdx(RegI64(Register64(rdx))),
#endif
#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86)
      specific_eax(RegI32(eax)),
      specific_ecx(RegI32(ecx)),
      specific_edx(RegI32(edx)),
#endif
#ifdef JS_CODEGEN_X86
      singleByteRegs_(GeneralRegisterSet(Registers::SingleByteRegs)),
      abiReturnRegI64(RegI64(Register64(edx, eax))),
#endif
#ifdef JS_CODEGEN_ARM
      abiReturnRegI64(ReturnReg64),
#endif
      joinRegI32(RegI32(ReturnReg)),
      joinRegI64(RegI64(ReturnReg64)),
      joinRegF32(RegF32(ReturnFloat32Reg)),
      joinRegF64(RegF64(ReturnDoubleReg))
{
    // jit/RegisterAllocator.h: RegisterAllocator::RegisterAllocator()

#if defined(JS_CODEGEN_X64)
    availGPR_.take(HeapReg);
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    availGPR_.take(HeapReg);
    availGPR_.take(GlobalReg);
    availGPR_.take(ScratchRegARM);
#elif defined(JS_CODEGEN_ARM64)
    availGPR_.take(HeapReg);
    availGPR_.take(HeapLenReg);
    availGPR_.take(GlobalReg);
#elif defined(JS_CODEGEN_X86)
    availGPR_.take(ScratchRegX86);
#endif

    labelPool_.setAllocator(alloc_);
}

bool
BaseCompiler::init()
{
    if (!SigDD_.append(ValType::F64) || !SigDD_.append(ValType::F64))
        return false;
    if (!SigD_.append(ValType::F64))
        return false;
    if (!SigF_.append(ValType::F32))
        return false;
    if (!SigI_.append(ValType::I32))
        return false;
    if (!SigI64I64_.append(ValType::I64) || !SigI64I64_.append(ValType::I64))
        return false;

    const ValTypeVector& args = func_.sig().args();

    // localInfo_ contains an entry for every local in locals_, followed by
    // entries for special locals. Currently the only special local is the TLS
    // pointer.
    tlsSlot_ = locals_.length();
    if (!localInfo_.resize(locals_.length() + 1))
        return false;

    localSize_ = 0;

    for (ABIArgIter<const ValTypeVector> i(args); !i.done(); i++) {
        Local& l = localInfo_[i.index()];
        switch (i.mirType()) {
          case MIRType::Int32:
            if (i->argInRegister())
                l.init(MIRType::Int32, pushLocal(4));
            else
                l.init(MIRType::Int32, -(i->offsetFromArgBase() + sizeof(AsmJSFrame)));
            break;
          case MIRType::Int64:
            if (i->argInRegister())
                l.init(MIRType::Int64, pushLocal(8));
            else
                l.init(MIRType::Int64, -(i->offsetFromArgBase() + sizeof(AsmJSFrame)));
            break;
          case MIRType::Double:
            if (i->argInRegister())
                l.init(MIRType::Double, pushLocal(8));
            else
                l.init(MIRType::Double, -(i->offsetFromArgBase() + sizeof(AsmJSFrame)));
            break;
          case MIRType::Float32:
            if (i->argInRegister())
                l.init(MIRType::Float32, pushLocal(4));
            else
                l.init(MIRType::Float32, -(i->offsetFromArgBase() + sizeof(AsmJSFrame)));
            break;
          default:
            MOZ_CRASH("Argument type");
        }
    }

    // Reserve a stack slot for the TLS pointer outside the varLow..varHigh
    // range so it isn't zero-filled like the normal locals.
    localInfo_[tlsSlot_].init(MIRType::Pointer, pushLocal(sizeof(void*)));

    varLow_ = localSize_;

    for (size_t i = args.length(); i < locals_.length(); i++) {
        Local& l = localInfo_[i];
        switch (locals_[i]) {
          case ValType::I32:
            l.init(MIRType::Int32, pushLocal(4));
            break;
          case ValType::F32:
            l.init(MIRType::Float32, pushLocal(4));
            break;
          case ValType::F64:
            l.init(MIRType::Double, pushLocal(8));
            break;
          case ValType::I64:
            l.init(MIRType::Int64, pushLocal(8));
            break;
          default:
            MOZ_CRASH("Compiler bug: Unexpected local type");
        }
    }

    varHigh_ = localSize_;

    localSize_ = AlignBytes(localSize_, 16u);

    addInterruptCheck();

    return true;
}

void
BaseCompiler::finish()
{
    MOZ_ASSERT(done(), "all bytes must be consumed");
    MOZ_ASSERT(func_.callSiteLineNums().length() == lastReadCallSite_);

    masm.flushBuffer();
}

static LiveRegisterSet
volatileReturnGPR()
{
    GeneralRegisterSet rtn;
    rtn.addAllocatable(ReturnReg);
    return LiveRegisterSet(RegisterSet::VolatileNot(RegisterSet(rtn, FloatRegisterSet())));
}

LiveRegisterSet BaseCompiler::VolatileReturnGPR = volatileReturnGPR();

} // wasm
} // js

bool
js::wasm::BaselineCanCompile(const FunctionGenerator* fg)
{
    // On all platforms we require signals for AsmJS/Wasm.
    // If we made it this far we must have signals.
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

#if defined(JS_CODEGEN_ARM)
    // Simplifying assumption: require SDIV and UDIV.
    //
    // I have no good data on ARM populations allowing me to say that
    // X% of devices in the market implement SDIV and UDIV.  However,
    // they are definitely implemented on the Cortex-A7 and Cortex-A15
    // and on all ARMv8 systems.
    if (!HasIDIV())
        return false;
#endif

#if defined(JS_CODEGEN_X64) || defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_ARM)
    if (fg->usesAtomics())
        return false;

    if (fg->usesSimd())
        return false;

    return true;
#else
    return false;
#endif
}

bool
js::wasm::BaselineCompileFunction(IonCompileTask* task)
{
    MOZ_ASSERT(task->mode() == IonCompileTask::CompileMode::Baseline);

    const FuncBytes& func = task->func();
    FuncCompileResults& results = task->results();

    Decoder d(func.bytes());

    // Build the local types vector.

    ValTypeVector locals;
    if (!locals.appendAll(func.sig().args()))
        return false;
    if (!DecodeLocalEntries(d, &locals))
        return false;

    // The MacroAssembler will sometimes access the jitContext.

    JitContext jitContext(&results.alloc());

    // One-pass baseline compilation.

    BaseCompiler f(task->mg(), d, func, locals, results);
    if (!f.init())
        return false;

    if (!f.emitFunction())
        return false;

    f.finish();

    return true;
}

#undef QUOT_REM_I64_CALLOUT
#undef I64_TO_FLOAT_CALLOUT
#undef FLOAT_TO_I64_CALLOUT
