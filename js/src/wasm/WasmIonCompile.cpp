/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#include "wasm/WasmIonCompile.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/CodeGenerator.h"

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBinaryIterator.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::DebugOnly;
using mozilla::IsPowerOfTwo;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

namespace {

typedef Vector<MBasicBlock*, 8, SystemAllocPolicy> BlockVector;

struct IonCompilePolicy : OpIterPolicy
{
    // Producing output is what we're all about here.
    static const bool Output = true;

    // We store SSA definitions in the value stack.
    typedef MDefinition* Value;

    // We store loop headers and then/else blocks in the control flow stack.
    typedef MBasicBlock* ControlItem;
};

typedef OpIter<IonCompilePolicy> IonOpIter;

class FunctionCompiler;

// TlsUsage describes how the TLS register is used during a function call.

enum class TlsUsage {
    Unused,     // No particular action is taken with respect to the TLS register.
    Need,       // The TLS register must be reloaded just before the call.
    CallerSaved // Same, plus space must be allocated to save/restore the TLS
                // register.
};

static bool
NeedsTls(TlsUsage usage) {
    return usage == TlsUsage::Need || usage == TlsUsage::CallerSaved;
}

// CallCompileState describes a call that is being compiled. Due to expression
// nesting, multiple calls can be in the middle of compilation at the same time
// and these are tracked in a stack by FunctionCompiler.

class CallCompileState
{
    // The line or bytecode of the call.
    uint32_t lineOrBytecode_;

    // A generator object that is passed each argument as it is compiled.
    ABIArgGenerator abi_;

    // The maximum number of bytes used by "child" calls, i.e., calls that occur
    // while evaluating the arguments of the call represented by this
    // CallCompileState.
    uint32_t maxChildStackBytes_;

    // Set by FunctionCompiler::finishCall(), tells the MWasmCall by how
    // much to bump the stack pointer before making the call. See
    // FunctionCompiler::startCall() comment below.
    uint32_t spIncrement_;

    // Set by FunctionCompiler::finishCall(), tells a potentially-inter-module
    // call the offset of the reserved space in which it can save the caller's
    // WasmTlsReg.
    uint32_t tlsStackOffset_;

    // Accumulates the register arguments while compiling arguments.
    MWasmCall::Args regArgs_;

    // Reserved argument for passing Instance* to builtin instance method calls.
    ABIArg instanceArg_;

    // Accumulates the stack arguments while compiling arguments. This is only
    // necessary to track when childClobbers_ is true so that the stack offsets
    // can be updated.
    Vector<MWasmStackArg*, 0, SystemAllocPolicy> stackArgs_;

    // Set by child calls (i.e., calls that execute while evaluating a parent's
    // operands) to indicate that the child and parent call cannot reuse the
    // same stack space -- the parent must store its stack arguments below the
    // child's and increment sp when performing its call.
    bool childClobbers_;

    // Only FunctionCompiler should be directly manipulating CallCompileState.
    friend class FunctionCompiler;

  public:
    CallCompileState(FunctionCompiler& f, uint32_t lineOrBytecode)
      : lineOrBytecode_(lineOrBytecode),
        maxChildStackBytes_(0),
        spIncrement_(0),
        tlsStackOffset_(MWasmCall::DontSaveTls),
        childClobbers_(false)
    { }
};

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph.
class FunctionCompiler
{
    struct ControlFlowPatch {
        MControlInstruction* ins;
        uint32_t index;
        ControlFlowPatch(MControlInstruction* ins, uint32_t index)
          : ins(ins),
            index(index)
        {}
    };

    typedef Vector<ControlFlowPatch, 0, SystemAllocPolicy> ControlFlowPatchVector;
    typedef Vector<ControlFlowPatchVector, 0, SystemAllocPolicy> ControlFlowPatchsVector;
    typedef Vector<CallCompileState*, 0, SystemAllocPolicy> CallCompileStateVector;

    const ModuleEnvironment&   env_;
    IonOpIter                  iter_;
    const FuncBytes&           func_;
    const ValTypeVector&       locals_;
    size_t                     lastReadCallSite_;

    TempAllocator&             alloc_;
    MIRGraph&                  graph_;
    const CompileInfo&         info_;
    MIRGenerator&              mirGen_;

    MBasicBlock*               curBlock_;
    CallCompileStateVector     callStack_;
    uint32_t                   maxStackArgBytes_;

    uint32_t                   loopDepth_;
    uint32_t                   blockDepth_;
    ControlFlowPatchsVector    blockPatches_;

    // TLS pointer argument to the current function.
    MWasmParameter*            tlsPointer_;

  public:
    FunctionCompiler(const ModuleEnvironment& env,
                     Decoder& decoder,
                     const FuncBytes& func,
                     const ValTypeVector& locals,
                     MIRGenerator& mirGen)
      : env_(env),
        iter_(decoder, func.lineOrBytecode()),
        func_(func),
        locals_(locals),
        lastReadCallSite_(0),
        alloc_(mirGen.alloc()),
        graph_(mirGen.graph()),
        info_(mirGen.info()),
        mirGen_(mirGen),
        curBlock_(nullptr),
        maxStackArgBytes_(0),
        loopDepth_(0),
        blockDepth_(0),
        tlsPointer_(nullptr)
    {}

    const ModuleEnvironment&   env() const   { return env_; }
    IonOpIter&                 iter()        { return iter_; }
    TempAllocator&             alloc() const { return alloc_; }
    const Sig&                 sig() const   { return func_.sig(); }

    TrapOffset trapOffset() const {
        return iter_.trapOffset();
    }
    Maybe<TrapOffset> trapIfNotAsmJS() const {
        return env_.isAsmJS() ? Nothing() : Some(iter_.trapOffset());
    }

    bool init()
    {
        // Prepare the entry block for MIR generation:

        const ValTypeVector& args = func_.sig().args();

        if (!mirGen_.ensureBallast())
            return false;
        if (!newBlock(/* prev */ nullptr, &curBlock_))
            return false;

        for (ABIArgValTypeIter i(args); !i.done(); i++) {
            MWasmParameter* ins = MWasmParameter::New(alloc(), *i, i.mirType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i.index()), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        // Set up a parameter that receives the hidden TLS pointer argument.
        tlsPointer_ = MWasmParameter::New(alloc(), ABIArg(WasmTlsReg), MIRType::Pointer);
        curBlock_->add(tlsPointer_);
        if (!mirGen_.ensureBallast())
            return false;

        for (size_t i = args.length(); i < locals_.length(); i++) {
            MInstruction* ins = nullptr;
            switch (locals_[i]) {
              case ValType::I32:
                ins = MConstant::New(alloc(), Int32Value(0), MIRType::Int32);
                break;
              case ValType::I64:
                ins = MConstant::NewInt64(alloc(), 0);
                break;
              case ValType::F32:
                ins = MConstant::New(alloc(), Float32Value(0.f), MIRType::Float32);
                break;
              case ValType::F64:
                ins = MConstant::New(alloc(), DoubleValue(0.0), MIRType::Double);
                break;
              case ValType::I8x16:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX16(0), MIRType::Int8x16);
                break;
              case ValType::I16x8:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX8(0), MIRType::Int16x8);
                break;
              case ValType::I32x4:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0), MIRType::Int32x4);
                break;
              case ValType::F32x4:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0.f), MIRType::Float32x4);
                break;
              case ValType::B8x16:
                // Bool8x16 uses the same data layout as Int8x16.
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX16(0), MIRType::Bool8x16);
                break;
              case ValType::B16x8:
                // Bool16x8 uses the same data layout as Int16x8.
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX8(0), MIRType::Bool16x8);
                break;
              case ValType::B32x4:
                // Bool32x4 uses the same data layout as Int32x4.
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0), MIRType::Bool32x4);
                break;
            }

            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        addInterruptCheck();

        return true;
    }

    void finish()
    {
        mirGen().initWasmMaxStackArgBytes(maxStackArgBytes_);

        MOZ_ASSERT(callStack_.empty());
        MOZ_ASSERT(loopDepth_ == 0);
        MOZ_ASSERT(blockDepth_ == 0);
#ifdef DEBUG
        for (ControlFlowPatchVector& patches : blockPatches_)
            MOZ_ASSERT(patches.empty());
#endif
        MOZ_ASSERT(inDeadCode());
        MOZ_ASSERT(done(), "all bytes must be consumed");
        MOZ_ASSERT(func_.callSiteLineNums().length() == lastReadCallSite_);
    }

    /************************* Read-only interface (after local scope setup) */

    MIRGenerator&       mirGen() const     { return mirGen_; }
    MIRGraph&           mirGraph() const   { return graph_; }
    const CompileInfo&  info() const       { return info_; }

    MDefinition* getLocalDef(unsigned slot)
    {
        if (inDeadCode())
            return nullptr;
        return curBlock_->getSlot(info().localSlot(slot));
    }

    const ValTypeVector& locals() const { return locals_; }

    /***************************** Code generation (after local scope setup) */

    MDefinition* constant(const SimdConstant& v, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        MInstruction* constant;
        constant = MSimdConstant::New(alloc(), v, type);
        curBlock_->add(constant);
        return constant;
    }

    MDefinition* constant(const Value& v, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        MConstant* constant = MConstant::New(alloc(), v, type);
        curBlock_->add(constant);
        return constant;
    }

    MDefinition* constant(float f)
    {
        if (inDeadCode())
            return nullptr;
        MConstant* constant = MConstant::NewRawFloat32(alloc(), f);
        curBlock_->add(constant);
        return constant;
    }

    MDefinition* constant(double d)
    {
        if (inDeadCode())
            return nullptr;
        MConstant* constant = MConstant::NewRawDouble(alloc(), d);
        curBlock_->add(constant);
        return constant;
    }

    MDefinition* constant(int64_t i)
    {
        if (inDeadCode())
            return nullptr;
        MConstant* constant = MConstant::NewInt64(alloc(), i);
        curBlock_->add(constant);
        return constant;
    }

    template <class T>
    MDefinition* unary(MDefinition* op)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::New(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* unary(MDefinition* op, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::New(alloc(), op, type);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* binary(MDefinition* lhs, MDefinition* rhs)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::New(alloc(), lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* binary(MDefinition* lhs, MDefinition* rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::New(alloc(), lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    bool mustPreserveNaN(MIRType type)
    {
        return IsFloatingPointType(type) && !env().isAsmJS();
    }

    MDefinition* sub(MDefinition* lhs, MDefinition* rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        // wasm can't fold x - 0.0 because of NaN with custom payloads.
        MSub* ins = MSub::New(alloc(), lhs, rhs, type, mustPreserveNaN(type));
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* unarySimd(MDefinition* input, MSimdUnaryArith::Operation op, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(input->type()) && input->type() == type);
        MInstruction* ins = MSimdUnaryArith::New(alloc(), input, op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* binarySimd(MDefinition* lhs, MDefinition* rhs, MSimdBinaryArith::Operation op,
                            MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(lhs->type()) && rhs->type() == lhs->type());
        MOZ_ASSERT(lhs->type() == type);
        return MSimdBinaryArith::AddLegalized(alloc(), curBlock_, lhs, rhs, op);
    }

    MDefinition* binarySimd(MDefinition* lhs, MDefinition* rhs, MSimdBinaryBitwise::Operation op,
                            MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(lhs->type()) && rhs->type() == lhs->type());
        MOZ_ASSERT(lhs->type() == type);
        auto* ins = MSimdBinaryBitwise::New(alloc(), lhs, rhs, op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* binarySimdComp(MDefinition* lhs, MDefinition* rhs, MSimdBinaryComp::Operation op,
                                SimdSign sign)
    {
        if (inDeadCode())
            return nullptr;

        return MSimdBinaryComp::AddLegalized(alloc(), curBlock_, lhs, rhs, op, sign);
    }

    MDefinition* binarySimdSaturating(MDefinition* lhs, MDefinition* rhs,
                                      MSimdBinarySaturating::Operation op, SimdSign sign)
    {
        if (inDeadCode())
            return nullptr;

        auto* ins = MSimdBinarySaturating::New(alloc(), lhs, rhs, op, sign);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* binarySimdShift(MDefinition* lhs, MDefinition* rhs, MSimdShift::Operation op)
    {
        if (inDeadCode())
            return nullptr;

        return MSimdShift::AddLegalized(alloc(), curBlock_, lhs, rhs, op);
    }

    MDefinition* swizzleSimd(MDefinition* vector, const uint8_t lanes[], MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(vector->type() == type);
        MSimdSwizzle* ins = MSimdSwizzle::New(alloc(), vector, lanes);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* shuffleSimd(MDefinition* lhs, MDefinition* rhs, const uint8_t lanes[],
                             MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(lhs->type() == type);
        MInstruction* ins = MSimdShuffle::New(alloc(), lhs, rhs, lanes);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* insertElementSimd(MDefinition* vec, MDefinition* val, unsigned lane, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(vec->type()) && vec->type() == type);
        MOZ_ASSERT(SimdTypeToLaneArgumentType(vec->type()) == val->type());
        MSimdInsertElement* ins = MSimdInsertElement::New(alloc(), vec, val, lane);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* selectSimd(MDefinition* mask, MDefinition* lhs, MDefinition* rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(mask->type()));
        MOZ_ASSERT(IsSimdType(lhs->type()) && rhs->type() == lhs->type());
        MOZ_ASSERT(lhs->type() == type);
        MSimdSelect* ins = MSimdSelect::New(alloc(), mask, lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* simdAllTrue(MDefinition* boolVector)
    {
        if (inDeadCode())
            return nullptr;

        MSimdAllTrue* ins = MSimdAllTrue::New(alloc(), boolVector, MIRType::Int32);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* simdAnyTrue(MDefinition* boolVector)
    {
        if (inDeadCode())
            return nullptr;

        MSimdAnyTrue* ins = MSimdAnyTrue::New(alloc(), boolVector, MIRType::Int32);
        curBlock_->add(ins);
        return ins;
    }

    // fromXXXBits()
    MDefinition* bitcastSimd(MDefinition* vec, MIRType from, MIRType to)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(vec->type() == from);
        MOZ_ASSERT(IsSimdType(from) && IsSimdType(to) && from != to);
        auto* ins = MSimdReinterpretCast::New(alloc(), vec, to);
        curBlock_->add(ins);
        return ins;
    }

    // Int <--> Float conversions.
    MDefinition* convertSimd(MDefinition* vec, MIRType from, MIRType to, SimdSign sign)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(from) && IsSimdType(to) && from != to);
        return MSimdConvert::AddLegalized(alloc(), curBlock_, vec, to, sign, trapOffset());
    }

    MDefinition* splatSimd(MDefinition* v, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(type));
        MOZ_ASSERT(SimdTypeToLaneArgumentType(type) == v->type());
        MSimdSplat* ins = MSimdSplat::New(alloc(), v, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* minMax(MDefinition* lhs, MDefinition* rhs, MIRType type, bool isMax) {
        if (inDeadCode())
            return nullptr;

        if (mustPreserveNaN(type)) {
            // Convert signaling NaN to quiet NaNs.
            MDefinition* zero = constant(DoubleValue(0.0), type);
            lhs = sub(lhs, zero, type);
            rhs = sub(rhs, zero, type);
        }

        MMinMax* ins = MMinMax::NewWasm(alloc(), lhs, rhs, type, isMax);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* mul(MDefinition* lhs, MDefinition* rhs, MIRType type, MMul::Mode mode)
    {
        if (inDeadCode())
            return nullptr;

        // wasm can't fold x * 1.0 because of NaN with custom payloads.
        auto* ins = MMul::NewWasm(alloc(), lhs, rhs, type, mode, mustPreserveNaN(type));
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* div(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        bool trapOnError = !env().isAsmJS();
        auto* ins = MDiv::New(alloc(), lhs, rhs, type, unsignd, trapOnError, trapOffset(),
                              mustPreserveNaN(type));
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* mod(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        bool trapOnError = !env().isAsmJS();
        auto* ins = MMod::New(alloc(), lhs, rhs, type, unsignd, trapOnError, trapOffset());
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* bitnot(MDefinition* op)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MBitNot::NewInt32(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* select(MDefinition* trueExpr, MDefinition* falseExpr, MDefinition* condExpr)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MWasmSelect::New(alloc(), trueExpr, falseExpr, condExpr);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* extendI32(MDefinition* op, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MExtendInt32ToInt64::New(alloc(), op, isUnsigned);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* convertI64ToFloatingPoint(MDefinition* op, MIRType type, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MInt64ToFloatingPoint::New(alloc(), op, type, isUnsigned);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* rotate(MDefinition* input, MDefinition* count, MIRType type, bool left)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MRotate::New(alloc(), input, count, type, left);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* truncate(MDefinition* op, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = T::New(alloc(), op, isUnsigned, trapOffset());
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* compare(MDefinition* lhs, MDefinition* rhs, JSOp op, MCompare::CompareType type)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MCompare::New(alloc(), lhs, rhs, op, type);
        curBlock_->add(ins);
        return ins;
    }

    void assign(unsigned slot, MDefinition* def)
    {
        if (inDeadCode())
            return;
        curBlock_->setSlot(info().localSlot(slot), def);
    }

  private:
    void checkOffsetAndBounds(MemoryAccessDesc* access, MDefinition** base)
    {
        // If the offset is bigger than the guard region, a separate instruction
        // is necessary to add the offset to the base and check for overflow.
        if (access->offset() >= OffsetGuardLimit || !JitOptions.wasmFoldOffsets) {
            auto* ins = MWasmAddOffset::New(alloc(), *base, access->offset(), trapOffset());
            curBlock_->add(ins);

            *base = ins;
            access->clearOffset();
        }

#ifndef WASM_HUGE_MEMORY
        curBlock_->add(MWasmBoundsCheck::New(alloc(), *base, trapOffset()));
#endif
    }

  public:
    MDefinition* load(MDefinition* base, MemoryAccessDesc access, ValType result)
    {
        if (inDeadCode())
            return nullptr;

        MInstruction* load = nullptr;
        if (access.isPlainAsmJS()) {
            MOZ_ASSERT(access.offset() == 0);
            load = MAsmJSLoadHeap::New(alloc(), base, access.type());
        } else {
            checkOffsetAndBounds(&access, &base);
            load = MWasmLoad::New(alloc(), base, access, ToMIRType(result));
        }

        curBlock_->add(load);
        return load;
    }

    void store(MDefinition* base, MemoryAccessDesc access, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MInstruction* store = nullptr;
        if (access.isPlainAsmJS()) {
            MOZ_ASSERT(access.offset() == 0);
            store = MAsmJSStoreHeap::New(alloc(), base, access.type(), v);
        } else {
            checkOffsetAndBounds(&access, &base);
            store = MWasmStore::New(alloc(), base, access, v);
        }

        curBlock_->add(store);
    }

    MDefinition* atomicCompareExchangeHeap(MDefinition* base, MemoryAccessDesc access,
                                           MDefinition* oldv, MDefinition* newv)
    {
        if (inDeadCode())
            return nullptr;

        checkOffsetAndBounds(&access, &base);
        auto* cas = MAsmJSCompareExchangeHeap::New(alloc(), base, access, oldv, newv, tlsPointer_);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicExchangeHeap(MDefinition* base, MemoryAccessDesc access,
                                    MDefinition* value)
    {
        if (inDeadCode())
            return nullptr;

        checkOffsetAndBounds(&access, &base);
        auto* cas = MAsmJSAtomicExchangeHeap::New(alloc(), base, access, value, tlsPointer_);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicBinopHeap(js::jit::AtomicOp op,
                                 MDefinition* base, MemoryAccessDesc access,
                                 MDefinition* v)
    {
        if (inDeadCode())
            return nullptr;

        checkOffsetAndBounds(&access, &base);
        auto* binop = MAsmJSAtomicBinopHeap::New(alloc(), op, base, access, v, tlsPointer_);
        curBlock_->add(binop);
        return binop;
    }

    MDefinition* loadGlobalVar(unsigned globalDataOffset, bool isConst, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        auto* load = MWasmLoadGlobalVar::New(alloc(), type, globalDataOffset, isConst);
        curBlock_->add(load);
        return load;
    }

    void storeGlobalVar(uint32_t globalDataOffset, MDefinition* v)
    {
        if (inDeadCode())
            return;
        curBlock_->add(MWasmStoreGlobalVar::New(alloc(), globalDataOffset, v));
    }

    void addInterruptCheck()
    {
        // We rely on signal handlers for interrupts on Asm.JS/Wasm
        MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());
    }

    MDefinition* extractSimdElement(unsigned lane, MDefinition* base, MIRType type, SimdSign sign)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(base->type()));
        MOZ_ASSERT(!IsSimdType(type));
        auto* ins = MSimdExtractElement::New(alloc(), base, type, lane, sign);
        curBlock_->add(ins);
        return ins;
    }

    template<typename T>
    MDefinition* constructSimd(MDefinition* x, MDefinition* y, MDefinition* z, MDefinition* w,
                               MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(type));
        T* ins = T::New(alloc(), type, x, y, z, w);
        curBlock_->add(ins);
        return ins;
    }

    /***************************************************************** Calls */

    // The IonMonkey backend maintains a single stack offset (from the stack
    // pointer to the base of the frame) by adding the total amount of spill
    // space required plus the maximum stack required for argument passing.
    // Since we do not use IonMonkey's MPrepareCall/MPassArg/MCall, we must
    // manually accumulate, for the entire function, the maximum required stack
    // space for argument passing. (This is passed to the CodeGenerator via
    // MIRGenerator::maxWasmStackArgBytes.) Naively, this would just be the
    // maximum of the stack space required for each individual call (as
    // determined by the call ABI). However, as an optimization, arguments are
    // stored to the stack immediately after evaluation (to decrease live
    // ranges and reduce spilling). This introduces the complexity that,
    // between evaluating an argument and making the call, another argument
    // evaluation could perform a call that also needs to store to the stack.
    // When this occurs childClobbers_ = true and the parent expression's
    // arguments are stored above the maximum depth clobbered by a child
    // expression.

    bool startCall(CallCompileState* call)
    {
        // Always push calls to maintain the invariant that if we're inDeadCode
        // in finishCall, we have something to pop.
        return callStack_.append(call);
    }

    bool passInstance(CallCompileState* args)
    {
        if (inDeadCode())
            return true;

        // Should only pass an instance once.
        MOZ_ASSERT(args->instanceArg_ == ABIArg());
        args->instanceArg_ = args->abi_.next(MIRType::Pointer);
        return true;
    }

    bool passArg(MDefinition* argDef, ValType type, CallCompileState* call)
    {
        if (inDeadCode())
            return true;

        ABIArg arg = call->abi_.next(ToMIRType(type));
        switch (arg.kind()) {
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR: {
            auto mirLow = MWrapInt64ToInt32::New(alloc(), argDef, /* bottomHalf = */ true);
            curBlock_->add(mirLow);
            auto mirHigh = MWrapInt64ToInt32::New(alloc(), argDef, /* bottomHalf = */ false);
            curBlock_->add(mirHigh);
            return call->regArgs_.append(MWasmCall::Arg(AnyRegister(arg.gpr64().low), mirLow)) &&
                   call->regArgs_.append(MWasmCall::Arg(AnyRegister(arg.gpr64().high), mirHigh));
          }
#endif
          case ABIArg::GPR:
          case ABIArg::FPU:
            return call->regArgs_.append(MWasmCall::Arg(arg.reg(), argDef));
          case ABIArg::Stack: {
            auto* mir = MWasmStackArg::New(alloc(), arg.offsetFromArgBase(), argDef);
            curBlock_->add(mir);
            return call->stackArgs_.append(mir);
          }
          default:
            MOZ_CRASH("Unknown ABIArg kind.");
        }
    }

    void propagateMaxStackArgBytes(uint32_t stackBytes)
    {
        if (callStack_.empty()) {
            // Outermost call
            maxStackArgBytes_ = Max(maxStackArgBytes_, stackBytes);
            return;
        }

        // Non-outermost call
        CallCompileState* outer = callStack_.back();
        outer->maxChildStackBytes_ = Max(outer->maxChildStackBytes_, stackBytes);
        if (stackBytes && !outer->stackArgs_.empty())
            outer->childClobbers_ = true;
    }

    bool finishCall(CallCompileState* call, TlsUsage tls)
    {
        MOZ_ALWAYS_TRUE(callStack_.popCopy() == call);

        if (inDeadCode()) {
            propagateMaxStackArgBytes(call->maxChildStackBytes_);
            return true;
        }

        if (NeedsTls(tls)) {
            if (!call->regArgs_.append(MWasmCall::Arg(AnyRegister(WasmTlsReg), tlsPointer_)))
                return false;
        }

        uint32_t stackBytes = call->abi_.stackBytesConsumedSoFar();

        // If this is a potentially-inter-module call, allocate an extra word of
        // stack space to save/restore the caller's WasmTlsReg during the call.
        // Record the stack offset before including spIncrement since MWasmCall
        // will use this offset after having bumped the stack pointer.
        if (tls == TlsUsage::CallerSaved) {
            call->tlsStackOffset_ = stackBytes;
            stackBytes += sizeof(void*);
        }

        if (call->childClobbers_) {
            call->spIncrement_ = AlignBytes(call->maxChildStackBytes_, WasmStackAlignment);
            for (MWasmStackArg* stackArg : call->stackArgs_)
                stackArg->incrementOffset(call->spIncrement_);

            // If instanceArg_ is not initialized then instanceArg_.kind() != ABIArg::Stack
            if (call->instanceArg_.kind() == ABIArg::Stack) {
                call->instanceArg_ = ABIArg(call->instanceArg_.offsetFromArgBase() +
                                            call->spIncrement_);
            }

            stackBytes += call->spIncrement_;
        } else {
            call->spIncrement_ = 0;
            stackBytes = Max(stackBytes, call->maxChildStackBytes_);
        }

        propagateMaxStackArgBytes(stackBytes);
        return true;
    }

    bool callDirect(const Sig& sig, uint32_t funcIndex, const CallCompileState& call,
                    MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        CallSiteDesc desc(call.lineOrBytecode_, CallSiteDesc::Func);
        MIRType ret = ToMIRType(sig.ret());
        auto callee = CalleeDesc::function(funcIndex);
        auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_, ret,
                                   call.spIncrement_, MWasmCall::DontSaveTls);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

    bool callIndirect(uint32_t sigIndex, MDefinition* index, const CallCompileState& call,
                      MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        const SigWithId& sig = env_.sigs[sigIndex];

        CalleeDesc callee;
        if (env_.isAsmJS()) {
            MOZ_ASSERT(sig.id.kind() == SigIdDesc::Kind::None);
            const TableDesc& table = env_.tables[env_.asmJSSigToTableIndex[sigIndex]];
            MOZ_ASSERT(IsPowerOfTwo(table.limits.initial));
            MOZ_ASSERT(!table.external);
            MOZ_ASSERT(call.tlsStackOffset_ == MWasmCall::DontSaveTls);

            MConstant* mask = MConstant::New(alloc(), Int32Value(table.limits.initial - 1));
            curBlock_->add(mask);
            MBitAnd* maskedIndex = MBitAnd::New(alloc(), index, mask, MIRType::Int32);
            curBlock_->add(maskedIndex);

            index = maskedIndex;
            callee = CalleeDesc::asmJSTable(table);
        } else {
            MOZ_ASSERT(sig.id.kind() != SigIdDesc::Kind::None);
            MOZ_ASSERT(env_.tables.length() == 1);
            const TableDesc& table = env_.tables[0];
            MOZ_ASSERT(table.external == (call.tlsStackOffset_ != MWasmCall::DontSaveTls));

            callee = CalleeDesc::wasmTable(table, sig.id);
        }

        CallSiteDesc desc(call.lineOrBytecode_, CallSiteDesc::Dynamic);
        auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_, ToMIRType(sig.ret()),
                                   call.spIncrement_, call.tlsStackOffset_, index);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

    bool callImport(unsigned globalDataOffset, const CallCompileState& call, ExprType ret,
                    MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MOZ_ASSERT(call.tlsStackOffset_ != MWasmCall::DontSaveTls);

        CallSiteDesc desc(call.lineOrBytecode_, CallSiteDesc::Dynamic);
        auto callee = CalleeDesc::import(globalDataOffset);
        auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_, ToMIRType(ret),
                                   call.spIncrement_, call.tlsStackOffset_);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

    bool builtinCall(SymbolicAddress builtin, const CallCompileState& call, ValType ret,
                     MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        CallSiteDesc desc(call.lineOrBytecode_, CallSiteDesc::Symbolic);
        auto callee = CalleeDesc::builtin(builtin);
        auto* ins = MWasmCall::New(alloc(), desc, callee, call.regArgs_, ToMIRType(ret),
                                   call.spIncrement_, MWasmCall::DontSaveTls);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

    bool builtinInstanceMethodCall(SymbolicAddress builtin, const CallCompileState& call,
                                   ValType ret, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        CallSiteDesc desc(call.lineOrBytecode_, CallSiteDesc::Symbolic);
        auto* ins = MWasmCall::NewBuiltinInstanceMethodCall(alloc(), desc, builtin,
                                                            call.instanceArg_, call.regArgs_,
                                                            ToMIRType(ret), call.spIncrement_,
                                                            call.tlsStackOffset_);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

    /*********************************************** Control flow generation */

    inline bool inDeadCode() const {
        return curBlock_ == nullptr;
    }

    void returnExpr(MDefinition* operand)
    {
        if (inDeadCode())
            return;

        MWasmReturn* ins = MWasmReturn::New(alloc(), operand, tlsPointer_);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void returnVoid()
    {
        if (inDeadCode())
            return;

        MWasmReturnVoid* ins = MWasmReturnVoid::New(alloc(), tlsPointer_);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void unreachableTrap()
    {
        if (inDeadCode())
            return;

        auto* ins = MWasmTrap::New(alloc(), wasm::Trap::Unreachable, trapOffset());
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

  private:
    static bool hasPushed(MBasicBlock* block)
    {
        uint32_t numPushed = block->stackDepth() - block->info().firstStackSlot();
        MOZ_ASSERT(numPushed == 0 || numPushed == 1);
        return numPushed;
    }

    static MDefinition* peekPushedDef(MBasicBlock* block)
    {
        MOZ_ASSERT(hasPushed(block));
        return block->getSlot(block->stackDepth() - 1);
    }

  public:
    void pushDef(MDefinition* def)
    {
        if (inDeadCode())
            return;
        MOZ_ASSERT(!hasPushed(curBlock_));
        if (def && def->type() != MIRType::None)
            curBlock_->push(def);
    }

    MDefinition* popDefIfPushed()
    {
        if (!hasPushed(curBlock_))
            return nullptr;
        MDefinition* def = curBlock_->pop();
        MOZ_ASSERT(def->type() != MIRType::Value);
        return def;
    }

  private:
    void addJoinPredecessor(MDefinition* def, MBasicBlock** joinPred)
    {
        *joinPred = curBlock_;
        if (inDeadCode())
            return;
        pushDef(def);
    }

  public:
    bool branchAndStartThen(MDefinition* cond, MBasicBlock** elseBlock)
    {
        if (inDeadCode()) {
            *elseBlock = nullptr;
        } else {
            MBasicBlock* thenBlock;
            if (!newBlock(curBlock_, &thenBlock))
                return false;
            if (!newBlock(curBlock_, elseBlock))
                return false;

            curBlock_->end(MTest::New(alloc(), cond, thenBlock, *elseBlock));

            curBlock_ = thenBlock;
            mirGraph().moveBlockToEnd(curBlock_);
        }

        return startBlock();
    }

    bool switchToElse(MBasicBlock* elseBlock, MBasicBlock** thenJoinPred)
    {
        MDefinition* ifDef;
        if (!finishBlock(&ifDef))
            return false;

        if (!elseBlock) {
            *thenJoinPred = nullptr;
        } else {
            addJoinPredecessor(ifDef, thenJoinPred);

            curBlock_ = elseBlock;
            mirGraph().moveBlockToEnd(curBlock_);
        }

        return startBlock();
    }

    bool joinIfElse(MBasicBlock* thenJoinPred, MDefinition** def)
    {
        MDefinition* elseDef;
        if (!finishBlock(&elseDef))
            return false;

        if (!thenJoinPred && inDeadCode()) {
            *def = nullptr;
        } else {
            MBasicBlock* elseJoinPred;
            addJoinPredecessor(elseDef, &elseJoinPred);

            mozilla::Array<MBasicBlock*, 2> blocks;
            size_t numJoinPreds = 0;
            if (thenJoinPred)
                blocks[numJoinPreds++] = thenJoinPred;
            if (elseJoinPred)
                blocks[numJoinPreds++] = elseJoinPred;

            if (numJoinPreds == 0) {
                *def = nullptr;
                return true;
            }

            MBasicBlock* join;
            if (!goToNewBlock(blocks[0], &join))
                return false;
            for (size_t i = 1; i < numJoinPreds; ++i) {
                if (!goToExistingBlock(blocks[i], join))
                    return false;
            }

            curBlock_ = join;
            *def = popDefIfPushed();
        }

        return true;
    }

    bool startBlock()
    {
        MOZ_ASSERT_IF(blockDepth_ < blockPatches_.length(), blockPatches_[blockDepth_].empty());
        blockDepth_++;
        return true;
    }

    bool finishBlock(MDefinition** def)
    {
        MOZ_ASSERT(blockDepth_);
        uint32_t topLabel = --blockDepth_;
        return bindBranches(topLabel, def);
    }

    bool startLoop(MBasicBlock** loopHeader)
    {
        *loopHeader = nullptr;

        blockDepth_++;
        loopDepth_++;

        if (inDeadCode())
            return true;

        // Create the loop header.
        MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_ - 1);
        *loopHeader = MBasicBlock::New(mirGraph(), info(), curBlock_,
                                       MBasicBlock::PENDING_LOOP_HEADER);
        if (!*loopHeader)
            return false;

        (*loopHeader)->setLoopDepth(loopDepth_);
        mirGraph().addBlock(*loopHeader);
        curBlock_->end(MGoto::New(alloc(), *loopHeader));

        MBasicBlock* body;
        if (!goToNewBlock(*loopHeader, &body))
            return false;
        curBlock_ = body;
        return true;
    }

  private:
    void fixupRedundantPhis(MBasicBlock* b)
    {
        for (size_t i = 0, depth = b->stackDepth(); i < depth; i++) {
            MDefinition* def = b->getSlot(i);
            if (def->isUnused())
                b->setSlot(i, def->toPhi()->getOperand(0));
        }
    }

    bool setLoopBackedge(MBasicBlock* loopEntry, MBasicBlock* loopBody, MBasicBlock* backedge)
    {
        if (!loopEntry->setBackedgeWasm(backedge))
            return false;

        // Flag all redundant phis as unused.
        for (MPhiIterator phi = loopEntry->phisBegin(); phi != loopEntry->phisEnd(); phi++) {
            MOZ_ASSERT(phi->numOperands() == 2);
            if (phi->getOperand(0) == phi->getOperand(1))
                phi->setUnused();
        }

        // Fix up phis stored in the slots Vector of pending blocks.
        for (ControlFlowPatchVector& patches : blockPatches_) {
            for (ControlFlowPatch& p : patches) {
                MBasicBlock* block = p.ins->block();
                if (block->loopDepth() >= loopEntry->loopDepth())
                    fixupRedundantPhis(block);
            }
        }

        // The loop body, if any, might be referencing recycled phis too.
        if (loopBody)
            fixupRedundantPhis(loopBody);

        // Discard redundant phis and add to the free list.
        for (MPhiIterator phi = loopEntry->phisBegin(); phi != loopEntry->phisEnd(); ) {
            MPhi* entryDef = *phi++;
            if (!entryDef->isUnused())
                continue;

            entryDef->justReplaceAllUsesWith(entryDef->getOperand(0));
            loopEntry->discardPhi(entryDef);
            mirGraph().addPhiToFreeList(entryDef);
        }

        return true;
    }

  public:
    bool closeLoop(MBasicBlock* loopHeader, MDefinition** loopResult)
    {
        MOZ_ASSERT(blockDepth_ >= 1);
        MOZ_ASSERT(loopDepth_);

        uint32_t headerLabel = blockDepth_ - 1;

        if (!loopHeader) {
            MOZ_ASSERT(inDeadCode());
            MOZ_ASSERT(headerLabel >= blockPatches_.length() || blockPatches_[headerLabel].empty());
            blockDepth_--;
            loopDepth_--;
            *loopResult = nullptr;
            return true;
        }

        // Op::Loop doesn't have an implicit backedge so temporarily set
        // aside the end of the loop body to bind backedges.
        MBasicBlock* loopBody = curBlock_;
        curBlock_ = nullptr;

        // As explained in bug 1253544, Ion apparently has an invariant that
        // there is only one backedge to loop headers. To handle wasm's ability
        // to have multiple backedges to the same loop header, we bind all those
        // branches as forward jumps to a single backward jump. This is
        // unfortunate but the optimizer is able to fold these into single jumps
        // to backedges.
        MDefinition* _;
        if (!bindBranches(headerLabel, &_))
            return false;

        MOZ_ASSERT(loopHeader->loopDepth() == loopDepth_);

        if (curBlock_) {
            // We're on the loop backedge block, created by bindBranches.
            if (hasPushed(curBlock_))
                curBlock_->pop();

            MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_);
            curBlock_->end(MGoto::New(alloc(), loopHeader));
            if (!setLoopBackedge(loopHeader, loopBody, curBlock_))
                return false;
        }

        curBlock_ = loopBody;

        loopDepth_--;

        // If the loop depth still at the inner loop body, correct it.
        if (curBlock_ && curBlock_->loopDepth() != loopDepth_) {
            MBasicBlock* out;
            if (!goToNewBlock(curBlock_, &out))
                return false;
            curBlock_ = out;
        }

        blockDepth_ -= 1;
        *loopResult = inDeadCode() ? nullptr : popDefIfPushed();
        return true;
    }

    bool addControlFlowPatch(MControlInstruction* ins, uint32_t relative, uint32_t index) {
        MOZ_ASSERT(relative < blockDepth_);
        uint32_t absolute = blockDepth_ - 1 - relative;

        if (absolute >= blockPatches_.length() && !blockPatches_.resize(absolute + 1))
            return false;

        return blockPatches_[absolute].append(ControlFlowPatch(ins, index));
    }

    bool br(uint32_t relativeDepth, MDefinition* maybeValue)
    {
        if (inDeadCode())
            return true;

        MGoto* jump = MGoto::New(alloc());
        if (!addControlFlowPatch(jump, relativeDepth, MGoto::TargetIndex))
            return false;

        pushDef(maybeValue);

        curBlock_->end(jump);
        curBlock_ = nullptr;
        return true;
    }

    bool brIf(uint32_t relativeDepth, MDefinition* maybeValue, MDefinition* condition)
    {
        if (inDeadCode())
            return true;

        MBasicBlock* joinBlock = nullptr;
        if (!newBlock(curBlock_, &joinBlock))
            return false;

        MTest* test = MTest::New(alloc(), condition, joinBlock);
        if (!addControlFlowPatch(test, relativeDepth, MTest::TrueBranchIndex))
            return false;

        pushDef(maybeValue);

        curBlock_->end(test);
        curBlock_ = joinBlock;
        return true;
    }

    bool brTable(MDefinition* operand, uint32_t defaultDepth, const Uint32Vector& depths,
                 MDefinition* maybeValue)
    {
        if (inDeadCode())
            return true;

        size_t numCases = depths.length();
        MOZ_ASSERT(numCases <= INT32_MAX);
        MOZ_ASSERT(numCases);

        MTableSwitch* table = MTableSwitch::New(alloc(), operand, 0, int32_t(numCases - 1));

        size_t defaultIndex;
        if (!table->addDefault(nullptr, &defaultIndex))
            return false;
        if (!addControlFlowPatch(table, defaultDepth, defaultIndex))
            return false;

        typedef HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy>
            IndexToCaseMap;

        IndexToCaseMap indexToCase;
        if (!indexToCase.init() || !indexToCase.put(defaultDepth, defaultIndex))
            return false;

        for (size_t i = 0; i < numCases; i++) {
            uint32_t depth = depths[i];

            size_t caseIndex;
            IndexToCaseMap::AddPtr p = indexToCase.lookupForAdd(depth);
            if (!p) {
                if (!table->addSuccessor(nullptr, &caseIndex))
                    return false;
                if (!addControlFlowPatch(table, depth, caseIndex))
                    return false;
                if (!indexToCase.add(p, depth, caseIndex))
                    return false;
            } else {
                caseIndex = p->value();
            }

            if (!table->addCase(caseIndex))
                return false;
        }

        pushDef(maybeValue);

        curBlock_->end(table);
        curBlock_ = nullptr;

        return true;
    }

    /************************************************************ DECODING ***/

    uint32_t readCallSiteLineOrBytecode() {
        if (!func_.callSiteLineNums().empty())
            return func_.callSiteLineNums()[lastReadCallSite_++];
        return iter_.trapOffset().bytecodeOffset;
    }

    bool done() const { return iter_.done(); }

    /*************************************************************************/
  private:
    bool newBlock(MBasicBlock* pred, MBasicBlock** block)
    {
        *block = MBasicBlock::New(mirGraph(), info(), pred, MBasicBlock::NORMAL);
        if (!*block)
            return false;
        mirGraph().addBlock(*block);
        (*block)->setLoopDepth(loopDepth_);
        return true;
    }

    bool goToNewBlock(MBasicBlock* pred, MBasicBlock** block)
    {
        if (!newBlock(pred, block))
            return false;
        pred->end(MGoto::New(alloc(), *block));
        return true;
    }

    bool goToExistingBlock(MBasicBlock* prev, MBasicBlock* next)
    {
        MOZ_ASSERT(prev);
        MOZ_ASSERT(next);
        prev->end(MGoto::New(alloc(), next));
        return next->addPredecessor(alloc(), prev);
    }

    bool bindBranches(uint32_t absolute, MDefinition** def)
    {
        if (absolute >= blockPatches_.length() || blockPatches_[absolute].empty()) {
            *def = inDeadCode() ? nullptr : popDefIfPushed();
            return true;
        }

        ControlFlowPatchVector& patches = blockPatches_[absolute];
        MControlInstruction* ins = patches[0].ins;
        MBasicBlock* pred = ins->block();

        MBasicBlock* join = nullptr;
        if (!newBlock(pred, &join))
            return false;

        pred->mark();
        ins->replaceSuccessor(patches[0].index, join);

        for (size_t i = 1; i < patches.length(); i++) {
            ins = patches[i].ins;

            pred = ins->block();
            if (!pred->isMarked()) {
                if (!join->addPredecessor(alloc(), pred))
                    return false;
                pred->mark();
            }

            ins->replaceSuccessor(patches[i].index, join);
        }

        MOZ_ASSERT_IF(curBlock_, !curBlock_->isMarked());
        for (uint32_t i = 0; i < join->numPredecessors(); i++)
            join->getPredecessor(i)->unmark();

        if (curBlock_ && !goToExistingBlock(curBlock_, join))
            return false;

        curBlock_ = join;

        *def = popDefIfPushed();

        patches.clear();
        return true;
    }
};

template <>
MDefinition* FunctionCompiler::unary<MToFloat32>(MDefinition* op)
{
    if (inDeadCode())
        return nullptr;
    auto* ins = MToFloat32::New(alloc(), op, mustPreserveNaN(op->type()));
    curBlock_->add(ins);
    return ins;
}

template <>
MDefinition* FunctionCompiler::unary<MNot>(MDefinition* op)
{
    if (inDeadCode())
        return nullptr;
    auto* ins = MNot::NewInt32(alloc(), op);
    curBlock_->add(ins);
    return ins;
}

template <>
MDefinition* FunctionCompiler::unary<MAbs>(MDefinition* op, MIRType type)
{
    if (inDeadCode())
        return nullptr;
    auto* ins = MAbs::NewWasm(alloc(), op, type);
    curBlock_->add(ins);
    return ins;
}

} // end anonymous namespace

static bool
EmitBlock(FunctionCompiler& f)
{
    return f.iter().readBlock() &&
           f.startBlock();
}

static bool
EmitLoop(FunctionCompiler& f)
{
    if (!f.iter().readLoop())
        return false;

    MBasicBlock* loopHeader;
    if (!f.startLoop(&loopHeader))
        return false;

    f.addInterruptCheck();

    f.iter().controlItem() = loopHeader;
    return true;
}

static bool
EmitIf(FunctionCompiler& f)
{
    MDefinition* condition = nullptr;
    if (!f.iter().readIf(&condition))
        return false;

    MBasicBlock* elseBlock;
    if (!f.branchAndStartThen(condition, &elseBlock))
        return false;

    f.iter().controlItem() = elseBlock;
    return true;
}

static bool
EmitElse(FunctionCompiler& f)
{
    MBasicBlock* block = f.iter().controlItem();

    ExprType thenType;
    MDefinition* thenValue;
    if (!f.iter().readElse(&thenType, &thenValue))
        return false;

    if (!IsVoid(thenType))
        f.pushDef(thenValue);

    if (!f.switchToElse(block, &f.iter().controlItem()))
        return false;

    return true;
}

static bool
EmitEnd(FunctionCompiler& f)
{
    MBasicBlock* block = f.iter().controlItem();

    LabelKind kind;
    ExprType type;
    MDefinition* value;
    if (!f.iter().readEnd(&kind, &type, &value))
        return false;

    if (!IsVoid(type))
        f.pushDef(value);

    MDefinition* def = nullptr;
    switch (kind) {
      case LabelKind::Block:
        if (!f.finishBlock(&def))
            return false;
        break;
      case LabelKind::Loop:
        if (!f.closeLoop(block, &def))
            return false;
        break;
      case LabelKind::Then:
      case LabelKind::UnreachableThen:
        // If we didn't see an Else, create a trivial else block so that we create
        // a diamond anyway, to preserve Ion invariants.
        if (!f.switchToElse(block, &block))
            return false;

        if (!f.joinIfElse(block, &def))
            return false;
        break;
      case LabelKind::Else:
        if (!f.joinIfElse(block, &def))
            return false;
        break;
    }

    if (!IsVoid(type)) {
        MOZ_ASSERT_IF(!f.inDeadCode(), def);
        f.iter().setResult(def);
    }

    return true;
}

static bool
EmitBr(FunctionCompiler& f)
{
    uint32_t relativeDepth;
    ExprType type;
    MDefinition* value;
    if (!f.iter().readBr(&relativeDepth, &type, &value))
        return false;

    if (IsVoid(type)) {
        if (!f.br(relativeDepth, nullptr))
            return false;
    } else {
        if (!f.br(relativeDepth, value))
            return false;
    }

    return true;
}

static bool
EmitBrIf(FunctionCompiler& f)
{
    uint32_t relativeDepth;
    ExprType type;
    MDefinition* value;
    MDefinition* condition;
    if (!f.iter().readBrIf(&relativeDepth, &type, &value, &condition))
        return false;

    if (IsVoid(type)) {
        if (!f.brIf(relativeDepth, nullptr, condition))
            return false;
    } else {
        if (!f.brIf(relativeDepth, value, condition))
            return false;
    }

    return true;
}

static bool
EmitBrTable(FunctionCompiler& f)
{
    uint32_t tableLength;
    ExprType type;
    MDefinition* value;
    MDefinition* index;
    if (!f.iter().readBrTable(&tableLength, &type, &value, &index))
        return false;

    Uint32Vector depths;
    if (!depths.reserve(tableLength))
        return false;

    for (size_t i = 0; i < tableLength; ++i) {
        uint32_t depth;
        if (!f.iter().readBrTableEntry(&type, &value, &depth))
            return false;
        depths.infallibleAppend(depth);
    }

    // Read the default label.
    uint32_t defaultDepth;
    if (!f.iter().readBrTableDefault(&type, &value, &defaultDepth))
        return false;

    MDefinition* maybeValue = IsVoid(type) ? nullptr : value;

    // If all the targets are the same, or there are no targets, we can just
    // use a goto. This is not just an optimization: MaybeFoldConditionBlock
    // assumes that tables have more than one successor.
    bool allSameDepth = true;
    for (uint32_t depth : depths) {
        if (depth != defaultDepth) {
            allSameDepth = false;
            break;
        }
    }

    if (allSameDepth)
        return f.br(defaultDepth, maybeValue);

    return f.brTable(index, defaultDepth, depths, maybeValue);
}

static bool
EmitReturn(FunctionCompiler& f)
{
    MDefinition* value;
    if (!f.iter().readReturn(&value))
        return false;

    if (f.inDeadCode())
        return true;

    if (IsVoid(f.sig().ret())) {
        f.returnVoid();
        return true;
    }

    f.returnExpr(value);
    return true;
}

static bool
EmitCallArgs(FunctionCompiler& f, const Sig& sig, TlsUsage tls, CallCompileState* call)
{
    MOZ_ASSERT(NeedsTls(tls));

    if (!f.startCall(call))
        return false;

    MDefinition* arg;
    const ValTypeVector& argTypes = sig.args();
    uint32_t numArgs = argTypes.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = argTypes[i];
        if (!f.iter().readCallArg(argType, numArgs, i, &arg))
            return false;
        if (!f.passArg(arg, argType, call))
            return false;
    }

    if (!f.iter().readCallArgsEnd(numArgs))
        return false;

    return f.finishCall(call, tls);
}

static bool
EmitCall(FunctionCompiler& f)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    uint32_t funcIndex;
    if (!f.iter().readCall(&funcIndex))
        return false;

    if (f.inDeadCode())
        return true;

    const Sig& sig = *f.env().funcSigs[funcIndex];
    bool import = f.env().funcIsImport(funcIndex);

    CallCompileState call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, import ? TlsUsage::CallerSaved : TlsUsage::Need, &call))
        return false;

    if (!f.iter().readCallReturn(sig.ret()))
        return false;

    MDefinition* def;
    if (import) {
        uint32_t globalDataOffset = f.env().funcImportGlobalDataOffsets[funcIndex];
        if (!f.callImport(globalDataOffset, call, sig.ret(), &def))
            return false;
    } else {
        if (!f.callDirect(sig, funcIndex, call, &def))
            return false;
    }

    if (IsVoid(sig.ret()))
        return true;

    f.iter().setResult(def);
    return true;
}

static bool
EmitCallIndirect(FunctionCompiler& f, bool oldStyle)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    uint32_t sigIndex;
    MDefinition* callee;
    if (oldStyle) {
        if (!f.iter().readOldCallIndirect(&sigIndex))
            return false;
    } else {
        if (!f.iter().readCallIndirect(&sigIndex, &callee))
            return false;
    }

    if (f.inDeadCode())
        return true;

    const Sig& sig = f.env().sigs[sigIndex];

    TlsUsage tls = !f.env().isAsmJS() && f.env().tables[0].external
                   ? TlsUsage::CallerSaved
                   : TlsUsage::Need;

    CallCompileState call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, tls, &call))
        return false;

    if (oldStyle) {
        if (!f.iter().readOldCallIndirectCallee(&callee))
            return false;
    }

    if (!f.iter().readCallReturn(sig.ret()))
        return false;

    MDefinition* def;
    if (!f.callIndirect(sigIndex, callee, call, &def))
        return false;

    if (IsVoid(sig.ret()))
        return true;

    f.iter().setResult(def);
    return true;
}

static bool
EmitGetLocal(FunctionCompiler& f)
{
    uint32_t id;
    if (!f.iter().readGetLocal(f.locals(), &id))
        return false;

    f.iter().setResult(f.getLocalDef(id));
    return true;
}

static bool
EmitSetLocal(FunctionCompiler& f)
{
    uint32_t id;
    MDefinition* value;
    if (!f.iter().readSetLocal(f.locals(), &id, &value))
        return false;

    f.assign(id, value);
    return true;
}

static bool
EmitTeeLocal(FunctionCompiler& f)
{
    uint32_t id;
    MDefinition* value;
    if (!f.iter().readTeeLocal(f.locals(), &id, &value))
        return false;

    f.assign(id, value);
    return true;
}

static bool
EmitGetGlobal(FunctionCompiler& f)
{
    uint32_t id;
    if (!f.iter().readGetGlobal(f.env().globals, &id))
        return false;

    const GlobalDesc& global = f.env().globals[id];
    if (!global.isConstant()) {
        f.iter().setResult(f.loadGlobalVar(global.offset(), !global.isMutable(),
                                           ToMIRType(global.type())));
        return true;
    }

    Val value = global.constantValue();
    MIRType mirType = ToMIRType(value.type());

    MDefinition* result;
    switch (value.type()) {
      case ValType::I32:
        result = f.constant(Int32Value(value.i32()), mirType);
        break;
      case ValType::I64:
        result = f.constant(int64_t(value.i64()));
        break;
      case ValType::F32:
        result = f.constant(value.f32());
        break;
      case ValType::F64:
        result = f.constant(value.f64());
        break;
      case ValType::I8x16:
        result = f.constant(SimdConstant::CreateX16(value.i8x16()), mirType);
        break;
      case ValType::I16x8:
        result = f.constant(SimdConstant::CreateX8(value.i16x8()), mirType);
        break;
      case ValType::I32x4:
        result = f.constant(SimdConstant::CreateX4(value.i32x4()), mirType);
        break;
      case ValType::F32x4:
        result = f.constant(SimdConstant::CreateX4(value.f32x4()), mirType);
        break;
      default:
        MOZ_CRASH("unexpected type in EmitGetGlobal");
    }

    f.iter().setResult(result);
    return true;
}

static bool
EmitSetGlobal(FunctionCompiler& f)
{
    uint32_t id;
    MDefinition* value;
    if (!f.iter().readSetGlobal(f.env().globals, &id, &value))
        return false;

    const GlobalDesc& global = f.env().globals[id];
    MOZ_ASSERT(global.isMutable());

    f.storeGlobalVar(global.offset(), value);
    return true;
}

static bool
EmitTeeGlobal(FunctionCompiler& f)
{
    uint32_t id;
    MDefinition* value;
    if (!f.iter().readTeeGlobal(f.env().globals, &id, &value))
        return false;

    const GlobalDesc& global = f.env().globals[id];
    MOZ_ASSERT(global.isMutable());

    f.storeGlobalVar(global.offset(), value);
    return true;
}

template <typename MIRClass>
static bool
EmitUnary(FunctionCompiler& f, ValType operandType)
{
    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    f.iter().setResult(f.unary<MIRClass>(input));
    return true;
}

template <typename MIRClass>
static bool
EmitConversion(FunctionCompiler& f, ValType operandType, ValType resultType)
{
    MDefinition* input;
    if (!f.iter().readConversion(operandType, resultType, &input))
        return false;

    f.iter().setResult(f.unary<MIRClass>(input));
    return true;
}

template <typename MIRClass>
static bool
EmitUnaryWithType(FunctionCompiler& f, ValType operandType, MIRType mirType)
{
    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    f.iter().setResult(f.unary<MIRClass>(input, mirType));
    return true;
}

template <typename MIRClass>
static bool
EmitConversionWithType(FunctionCompiler& f,
                       ValType operandType, ValType resultType, MIRType mirType)
{
    MDefinition* input;
    if (!f.iter().readConversion(operandType, resultType, &input))
        return false;

    f.iter().setResult(f.unary<MIRClass>(input, mirType));
    return true;
}

static bool
EmitTruncate(FunctionCompiler& f, ValType operandType, ValType resultType,
             bool isUnsigned)
{
    MDefinition* input;
    if (!f.iter().readConversion(operandType, resultType, &input))
        return false;

    if (resultType == ValType::I32) {
        if (f.env().isAsmJS())
            f.iter().setResult(f.unary<MTruncateToInt32>(input));
        else
            f.iter().setResult(f.truncate<MWasmTruncateToInt32>(input, isUnsigned));
    } else {
        MOZ_ASSERT(resultType == ValType::I64);
        MOZ_ASSERT(!f.env().isAsmJS());
        f.iter().setResult(f.truncate<MWasmTruncateToInt64>(input, isUnsigned));
    }
    return true;
}

static bool
EmitExtendI32(FunctionCompiler& f, bool isUnsigned)
{
    MDefinition* input;
    if (!f.iter().readConversion(ValType::I32, ValType::I64, &input))
        return false;

    f.iter().setResult(f.extendI32(input, isUnsigned));
    return true;
}

static bool
EmitConvertI64ToFloatingPoint(FunctionCompiler& f,
                              ValType resultType, MIRType mirType, bool isUnsigned)
{
    MDefinition* input;
    if (!f.iter().readConversion(ValType::I64, resultType, &input))
        return false;

    f.iter().setResult(f.convertI64ToFloatingPoint(input, mirType, isUnsigned));
    return true;
}

static bool
EmitReinterpret(FunctionCompiler& f, ValType resultType, ValType operandType, MIRType mirType)
{
    MDefinition* input;
    if (!f.iter().readConversion(operandType, resultType, &input))
        return false;

    f.iter().setResult(f.unary<MWasmReinterpret>(input, mirType));
    return true;
}

static bool
EmitAdd(FunctionCompiler& f, ValType type, MIRType mirType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(type, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binary<MAdd>(lhs, rhs, mirType));
    return true;
}

static bool
EmitSub(FunctionCompiler& f, ValType type, MIRType mirType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(type, &lhs, &rhs))
        return false;

    f.iter().setResult(f.sub(lhs, rhs, mirType));
    return true;
}

static bool
EmitRotate(FunctionCompiler& f, ValType type, bool isLeftRotation)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(type, &lhs, &rhs))
        return false;

    MDefinition* result = f.rotate(lhs, rhs, ToMIRType(type), isLeftRotation);
    f.iter().setResult(result);
    return true;
}

static bool
EmitBitNot(FunctionCompiler& f, ValType operandType)
{
    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    f.iter().setResult(f.bitnot(input));
    return true;
}

template <typename MIRClass>
static bool
EmitBitwise(FunctionCompiler& f, ValType operandType, MIRType mirType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binary<MIRClass>(lhs, rhs, mirType));
    return true;
}

static bool
EmitMul(FunctionCompiler& f, ValType operandType, MIRType mirType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.mul(lhs, rhs, mirType,
                       mirType == MIRType::Int32 ? MMul::Integer : MMul::Normal));
    return true;
}

static bool
EmitDiv(FunctionCompiler& f, ValType operandType, MIRType mirType, bool isUnsigned)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.div(lhs, rhs, mirType, isUnsigned));
    return true;
}

static bool
EmitRem(FunctionCompiler& f, ValType operandType, MIRType mirType, bool isUnsigned)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.mod(lhs, rhs, mirType, isUnsigned));
    return true;
}

static bool
EmitMinMax(FunctionCompiler& f, ValType operandType, MIRType mirType, bool isMax)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.minMax(lhs, rhs, mirType, isMax));
    return true;
}

static bool
EmitCopySign(FunctionCompiler& f, ValType operandType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binary<MCopySign>(lhs, rhs, ToMIRType(operandType)));
    return true;
}

static bool
EmitComparison(FunctionCompiler& f,
               ValType operandType, JSOp compareOp, MCompare::CompareType compareType)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readComparison(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.compare(lhs, rhs, compareOp, compareType));
    return true;
}

static bool
EmitSelect(FunctionCompiler& f)
{
    ValType type;
    MDefinition* trueValue;
    MDefinition* falseValue;
    MDefinition* condition;
    if (!f.iter().readSelect(&type, &trueValue, &falseValue, &condition))
        return false;

    f.iter().setResult(f.select(trueValue, falseValue, condition));
    return true;
}

static bool
EmitLoad(FunctionCompiler& f, ValType type, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    if (!f.iter().readLoad(type, Scalar::byteSize(viewType), &addr))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, f.trapIfNotAsmJS());
    f.iter().setResult(f.load(addr.base, access, type));
    return true;
}

static bool
EmitStore(FunctionCompiler& f, ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, f.trapIfNotAsmJS());

    f.store(addr.base, access, value);
    return true;
}

static bool
EmitTeeStore(FunctionCompiler& f, ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readTeeStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, f.trapIfNotAsmJS());

    f.store(addr.base, access, value);
    return true;
}

static bool
EmitTeeStoreWithCoercion(FunctionCompiler& f, ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readTeeStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    if (resultType == ValType::F32 && viewType == Scalar::Float64)
        value = f.unary<MToDouble>(value);
    else if (resultType == ValType::F64 && viewType == Scalar::Float32)
        value = f.unary<MToFloat32>(value);
    else
        MOZ_CRASH("unexpected coerced store");

    MemoryAccessDesc access(viewType, addr.align, addr.offset, f.trapIfNotAsmJS());

    f.store(addr.base, access, value);
    return true;
}

static bool
EmitUnaryMathBuiltinCall(FunctionCompiler& f, SymbolicAddress callee, ValType operandType)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    CallCompileState call(f, lineOrBytecode);
    if (!f.startCall(&call))
        return false;

    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    if (!f.passArg(input, operandType, &call))
        return false;

    if (!f.finishCall(&call, TlsUsage::Unused))
        return false;

    MDefinition* def;
    if (!f.builtinCall(callee, call, operandType, &def))
        return false;

    f.iter().setResult(def);
    return true;
}

static bool
EmitBinaryMathBuiltinCall(FunctionCompiler& f, SymbolicAddress callee, ValType operandType)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    CallCompileState call(f, lineOrBytecode);
    if (!f.startCall(&call))
        return false;

    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    if (!f.passArg(lhs, operandType, &call))
        return false;

    if (!f.passArg(rhs, operandType, &call))
        return false;

    if (!f.finishCall(&call, TlsUsage::Unused))
        return false;

    MDefinition* def;
    if (!f.builtinCall(callee, call, operandType, &def))
        return false;

    f.iter().setResult(def);
    return true;
}

static bool
EmitAtomicsLoad(FunctionCompiler& f)
{
    LinearMemoryAddress<MDefinition*> addr;
    Scalar::Type viewType;
    if (!f.iter().readAtomicLoad(&addr, &viewType))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()), 0,
                            MembarBeforeLoad, MembarAfterLoad);

    f.iter().setResult(f.load(addr.base, access, ValType::I32));
    return true;
}

static bool
EmitAtomicsStore(FunctionCompiler& f)
{
    LinearMemoryAddress<MDefinition*> addr;
    Scalar::Type viewType;
    MDefinition* value;
    if (!f.iter().readAtomicStore(&addr, &viewType, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()), 0,
                            MembarBeforeStore, MembarAfterStore);

    f.store(addr.base, access, value);
    f.iter().setResult(value);
    return true;
}

static bool
EmitAtomicsBinOp(FunctionCompiler& f)
{
    LinearMemoryAddress<MDefinition*> addr;
    Scalar::Type viewType;
    jit::AtomicOp op;
    MDefinition* value;
    if (!f.iter().readAtomicBinOp(&addr, &viewType, &op, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()));

    f.iter().setResult(f.atomicBinopHeap(op, addr.base, access, value));
    return true;
}

static bool
EmitAtomicsCompareExchange(FunctionCompiler& f)
{
    LinearMemoryAddress<MDefinition*> addr;
    Scalar::Type viewType;
    MDefinition* oldValue;
    MDefinition* newValue;
    if (!f.iter().readAtomicCompareExchange(&addr, &viewType, &oldValue, &newValue))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()));

    f.iter().setResult(f.atomicCompareExchangeHeap(addr.base, access, oldValue, newValue));
    return true;
}

static bool
EmitAtomicsExchange(FunctionCompiler& f)
{
    LinearMemoryAddress<MDefinition*> addr;
    Scalar::Type viewType;
    MDefinition* value;
    if (!f.iter().readAtomicExchange(&addr, &viewType, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()));

    f.iter().setResult(f.atomicExchangeHeap(addr.base, access, value));
    return true;
}

static bool
EmitSimdUnary(FunctionCompiler& f, ValType type, SimdOperation simdOp)
{
    MSimdUnaryArith::Operation op;
    switch (simdOp) {
      case SimdOperation::Fn_abs:
        op = MSimdUnaryArith::abs;
        break;
      case SimdOperation::Fn_neg:
        op = MSimdUnaryArith::neg;
        break;
      case SimdOperation::Fn_not:
        op = MSimdUnaryArith::not_;
        break;
      case SimdOperation::Fn_sqrt:
        op = MSimdUnaryArith::sqrt;
        break;
      case SimdOperation::Fn_reciprocalApproximation:
        op = MSimdUnaryArith::reciprocalApproximation;
        break;
      case SimdOperation::Fn_reciprocalSqrtApproximation:
        op = MSimdUnaryArith::reciprocalSqrtApproximation;
        break;
      default:
        MOZ_CRASH("not a simd unary arithmetic operation");
    }

    MDefinition* input;
    if (!f.iter().readUnary(type, &input))
        return false;

    f.iter().setResult(f.unarySimd(input, op, ToMIRType(type)));
    return true;
}

template<class OpKind>
inline bool
EmitSimdBinary(FunctionCompiler& f, ValType type, OpKind op)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(type, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binarySimd(lhs, rhs, op, ToMIRType(type)));
    return true;
}

static bool
EmitSimdBinaryComp(FunctionCompiler& f, ValType operandType, MSimdBinaryComp::Operation op,
                   SimdSign sign)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readSimdComparison(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binarySimdComp(lhs, rhs, op, sign));
    return true;
}

static bool
EmitSimdBinarySaturating(FunctionCompiler& f, ValType type, MSimdBinarySaturating::Operation op,
                         SimdSign sign)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(type, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binarySimdSaturating(lhs, rhs, op, sign));
    return true;
}

static bool
EmitSimdShift(FunctionCompiler& f, ValType operandType, MSimdShift::Operation op)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readSimdShiftByScalar(operandType, &lhs, &rhs))
        return false;

    f.iter().setResult(f.binarySimdShift(lhs, rhs, op));
    return true;
}

static ValType
SimdToLaneType(ValType type)
{
    switch (type) {
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:  return ValType::I32;
      case ValType::F32x4:  return ValType::F32;
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:  return ValType::I32; // Boolean lanes are Int32 in asm.
      case ValType::I32:
      case ValType::I64:
      case ValType::F32:
      case ValType::F64:
        break;
    }
    MOZ_CRASH("bad simd type");
}

static bool
EmitExtractLane(FunctionCompiler& f, ValType operandType, SimdSign sign)
{
    uint8_t lane;
    MDefinition* vector;
    if (!f.iter().readExtractLane(operandType, &lane, &vector))
        return false;

    f.iter().setResult(f.extractSimdElement(lane, vector,
                                            ToMIRType(SimdToLaneType(operandType)), sign));
    return true;
}

// Emit an I32 expression and then convert it to a boolean SIMD lane value, i.e. -1 or 0.
static MDefinition*
EmitSimdBooleanLaneExpr(FunctionCompiler& f, MDefinition* i32)
{
    // Compute !i32 - 1 to force the value range into {0, -1}.
    MDefinition* noti32 = f.unary<MNot>(i32);
    return f.binary<MSub>(noti32, f.constant(Int32Value(1), MIRType::Int32), MIRType::Int32);
}

static bool
EmitSimdReplaceLane(FunctionCompiler& f, ValType simdType)
{
    if (IsSimdBoolType(simdType))
        f.iter().setResult(EmitSimdBooleanLaneExpr(f, f.iter().getResult()));

    uint8_t lane;
    MDefinition* vector;
    MDefinition* scalar;
    if (!f.iter().readReplaceLane(simdType, &lane, &vector, &scalar))
        return false;

    f.iter().setResult(f.insertElementSimd(vector, scalar, lane, ToMIRType(simdType)));
    return true;
}

inline bool
EmitSimdBitcast(FunctionCompiler& f, ValType fromType, ValType toType)
{
    MDefinition* input;
    if (!f.iter().readConversion(fromType, toType, &input))
        return false;

    f.iter().setResult(f.bitcastSimd(input, ToMIRType(fromType), ToMIRType(toType)));
    return true;
}

inline bool
EmitSimdConvert(FunctionCompiler& f, ValType fromType, ValType toType, SimdSign sign)
{
    MDefinition* input;
    if (!f.iter().readConversion(fromType, toType, &input))
        return false;

    f.iter().setResult(f.convertSimd(input, ToMIRType(fromType), ToMIRType(toType), sign));
    return true;
}

static bool
EmitSimdSwizzle(FunctionCompiler& f, ValType simdType)
{
    uint8_t lanes[16];
    MDefinition* vector;
    if (!f.iter().readSwizzle(simdType, &lanes, &vector))
        return false;

    f.iter().setResult(f.swizzleSimd(vector, lanes, ToMIRType(simdType)));
    return true;
}

static bool
EmitSimdShuffle(FunctionCompiler& f, ValType simdType)
{
    uint8_t lanes[16];
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readShuffle(simdType, &lanes, &lhs, &rhs))
        return false;

    f.iter().setResult(f.shuffleSimd(lhs, rhs, lanes, ToMIRType(simdType)));
    return true;
}

static inline Scalar::Type
SimdExprTypeToViewType(ValType type, unsigned* defaultNumElems)
{
    switch (type) {
        case ValType::I8x16: *defaultNumElems = 16; return Scalar::Int8x16;
        case ValType::I16x8: *defaultNumElems = 8; return Scalar::Int16x8;
        case ValType::I32x4: *defaultNumElems = 4; return Scalar::Int32x4;
        case ValType::F32x4: *defaultNumElems = 4; return Scalar::Float32x4;
        default:              break;
    }
    MOZ_CRASH("type not handled in SimdExprTypeToViewType");
}

static bool
EmitSimdLoad(FunctionCompiler& f, ValType resultType, unsigned numElems)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(resultType, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    LinearMemoryAddress<MDefinition*> addr;
    if (!f.iter().readLoad(resultType, Scalar::byteSize(viewType), &addr))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()), numElems);

    f.iter().setResult(f.load(addr.base, access, resultType));
    return true;
}

static bool
EmitSimdStore(FunctionCompiler& f, ValType resultType, unsigned numElems)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(resultType, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readTeeStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    MemoryAccessDesc access(viewType, addr.align, addr.offset, Some(f.trapOffset()), numElems);

    f.store(addr.base, access, value);
    return true;
}

static bool
EmitSimdSelect(FunctionCompiler& f, ValType simdType)
{
    MDefinition* trueValue;
    MDefinition* falseValue;
    MDefinition* condition;
    if (!f.iter().readSimdSelect(simdType, &trueValue, &falseValue, &condition))
        return false;

    f.iter().setResult(f.selectSimd(condition, trueValue, falseValue,
                                    ToMIRType(simdType)));
    return true;
}

static bool
EmitSimdAllTrue(FunctionCompiler& f, ValType operandType)
{
    MDefinition* input;
    if (!f.iter().readSimdBooleanReduction(operandType, &input))
        return false;

    f.iter().setResult(f.simdAllTrue(input));
    return true;
}

static bool
EmitSimdAnyTrue(FunctionCompiler& f, ValType operandType)
{
    MDefinition* input;
    if (!f.iter().readSimdBooleanReduction(operandType, &input))
        return false;

    f.iter().setResult(f.simdAnyTrue(input));
    return true;
}

static bool
EmitSimdSplat(FunctionCompiler& f, ValType simdType)
{
    if (IsSimdBoolType(simdType))
        f.iter().setResult(EmitSimdBooleanLaneExpr(f, f.iter().getResult()));

    MDefinition* input;
    if (!f.iter().readSplat(simdType, &input))
        return false;

    f.iter().setResult(f.splatSimd(input, ToMIRType(simdType)));
    return true;
}

// Build a SIMD vector by inserting lanes one at a time into an initial constant.
static bool
EmitSimdChainedCtor(FunctionCompiler& f, ValType valType, MIRType type, const SimdConstant& init)
{
    const unsigned length = SimdTypeToLength(type);
    MDefinition* val = f.constant(init, type);
    for (unsigned i = 0; i < length; i++) {
        MDefinition* scalar = 0;
        if (!f.iter().readSimdCtorArg(ValType::I32, length, i, &scalar))
            return false;
        val = f.insertElementSimd(val, scalar, i, type);
    }
    if (!f.iter().readSimdCtorArgsEnd(length) || !f.iter().readSimdCtorReturn(valType))
        return false;
    f.iter().setResult(val);
    return true;
}

// Build a boolean SIMD vector by inserting lanes one at a time into an initial constant.
static bool
EmitSimdBooleanChainedCtor(FunctionCompiler& f, ValType valType, MIRType type,
                           const SimdConstant& init)
{
    const unsigned length = SimdTypeToLength(type);
    MDefinition* val = f.constant(init, type);
    for (unsigned i = 0; i < length; i++) {
        MDefinition* scalar = 0;
        if (!f.iter().readSimdCtorArg(ValType::I32, length, i, &scalar))
            return false;
        val = f.insertElementSimd(val, EmitSimdBooleanLaneExpr(f, scalar), i, type);
    }
    if (!f.iter().readSimdCtorArgsEnd(length) || !f.iter().readSimdCtorReturn(valType))
        return false;
    f.iter().setResult(val);
    return true;
}

static bool
EmitSimdCtor(FunctionCompiler& f, ValType type)
{
    if (!f.iter().readSimdCtor())
        return false;

    switch (type) {
      case ValType::I8x16:
        return EmitSimdChainedCtor(f, type, MIRType::Int8x16, SimdConstant::SplatX16(0));
      case ValType::I16x8:
        return EmitSimdChainedCtor(f, type, MIRType::Int16x8, SimdConstant::SplatX8(0));
      case ValType::I32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!f.iter().readSimdCtorArg(ValType::I32, 4, i, &args[i]))
                return false;
        }
        if (!f.iter().readSimdCtorArgsEnd(4) || !f.iter().readSimdCtorReturn(type))
            return false;
        f.iter().setResult(f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3],
                                                         MIRType::Int32x4));
        return true;
      }
      case ValType::F32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!f.iter().readSimdCtorArg(ValType::F32, 4, i, &args[i]))
                return false;
        }
        if (!f.iter().readSimdCtorArgsEnd(4) || !f.iter().readSimdCtorReturn(type))
            return false;
        f.iter().setResult(f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3],
                           MIRType::Float32x4));
        return true;
      }
      case ValType::B8x16:
        return EmitSimdBooleanChainedCtor(f, type, MIRType::Bool8x16, SimdConstant::SplatX16(0));
      case ValType::B16x8:
        return EmitSimdBooleanChainedCtor(f, type, MIRType::Bool16x8, SimdConstant::SplatX8(0));
      case ValType::B32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            MDefinition* i32;
            if (!f.iter().readSimdCtorArg(ValType::I32, 4, i, &i32))
                return false;
            args[i] = EmitSimdBooleanLaneExpr(f, i32);
        }
        if (!f.iter().readSimdCtorArgsEnd(4) || !f.iter().readSimdCtorReturn(type))
            return false;
        f.iter().setResult(f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3],
                           MIRType::Bool32x4));
        return true;
      }
      case ValType::I32:
      case ValType::I64:
      case ValType::F32:
      case ValType::F64:
        break;
    }
    MOZ_CRASH("unexpected SIMD type");
}

static bool
EmitSimdOp(FunctionCompiler& f, ValType type, SimdOperation op, SimdSign sign)
{
    switch (op) {
      case SimdOperation::Constructor:
        return EmitSimdCtor(f, type);
      case SimdOperation::Fn_extractLane:
        return EmitExtractLane(f, type, sign);
      case SimdOperation::Fn_replaceLane:
        return EmitSimdReplaceLane(f, type);
      case SimdOperation::Fn_check:
        MOZ_CRASH("only used in asm.js' type system");
      case SimdOperation::Fn_splat:
        return EmitSimdSplat(f, type);
      case SimdOperation::Fn_select:
        return EmitSimdSelect(f, type);
      case SimdOperation::Fn_swizzle:
        return EmitSimdSwizzle(f, type);
      case SimdOperation::Fn_shuffle:
        return EmitSimdShuffle(f, type);
      case SimdOperation::Fn_load:
        return EmitSimdLoad(f, type, 0);
      case SimdOperation::Fn_load1:
        return EmitSimdLoad(f, type, 1);
      case SimdOperation::Fn_load2:
        return EmitSimdLoad(f, type, 2);
      case SimdOperation::Fn_store:
        return EmitSimdStore(f, type, 0);
      case SimdOperation::Fn_store1:
        return EmitSimdStore(f, type, 1);
      case SimdOperation::Fn_store2:
        return EmitSimdStore(f, type, 2);
      case SimdOperation::Fn_allTrue:
        return EmitSimdAllTrue(f, type);
      case SimdOperation::Fn_anyTrue:
        return EmitSimdAnyTrue(f, type);
      case SimdOperation::Fn_abs:
      case SimdOperation::Fn_neg:
      case SimdOperation::Fn_not:
      case SimdOperation::Fn_sqrt:
      case SimdOperation::Fn_reciprocalApproximation:
      case SimdOperation::Fn_reciprocalSqrtApproximation:
        return EmitSimdUnary(f, type, op);
      case SimdOperation::Fn_shiftLeftByScalar:
        return EmitSimdShift(f, type, MSimdShift::lsh);
      case SimdOperation::Fn_shiftRightByScalar:
        return EmitSimdShift(f, type, MSimdShift::rshForSign(sign));
#define _CASE(OP) \
      case SimdOperation::Fn_##OP: \
        return EmitSimdBinaryComp(f, type, MSimdBinaryComp::OP, sign);
        FOREACH_COMP_SIMD_OP(_CASE)
#undef _CASE
      case SimdOperation::Fn_and:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::and_);
      case SimdOperation::Fn_or:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::or_);
      case SimdOperation::Fn_xor:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::xor_);
#define _CASE(OP) \
      case SimdOperation::Fn_##OP: \
        return EmitSimdBinary(f, type, MSimdBinaryArith::Op_##OP);
      FOREACH_NUMERIC_SIMD_BINOP(_CASE)
      FOREACH_FLOAT_SIMD_BINOP(_CASE)
#undef _CASE
      case SimdOperation::Fn_addSaturate:
        return EmitSimdBinarySaturating(f, type, MSimdBinarySaturating::add, sign);
      case SimdOperation::Fn_subSaturate:
        return EmitSimdBinarySaturating(f, type, MSimdBinarySaturating::sub, sign);
      case SimdOperation::Fn_fromFloat32x4:
        return EmitSimdConvert(f, ValType::F32x4, type, sign);
      case SimdOperation::Fn_fromInt32x4:
        return EmitSimdConvert(f, ValType::I32x4, type, SimdSign::Signed);
      case SimdOperation::Fn_fromUint32x4:
        return EmitSimdConvert(f, ValType::I32x4, type, SimdSign::Unsigned);
      case SimdOperation::Fn_fromInt8x16Bits:
      case SimdOperation::Fn_fromUint8x16Bits:
        return EmitSimdBitcast(f, ValType::I8x16, type);
      case SimdOperation::Fn_fromUint16x8Bits:
      case SimdOperation::Fn_fromInt16x8Bits:
        return EmitSimdBitcast(f, ValType::I16x8, type);
      case SimdOperation::Fn_fromInt32x4Bits:
      case SimdOperation::Fn_fromUint32x4Bits:
        return EmitSimdBitcast(f, ValType::I32x4, type);
      case SimdOperation::Fn_fromFloat32x4Bits:
        return EmitSimdBitcast(f, ValType::F32x4, type);
      case SimdOperation::Fn_load3:
      case SimdOperation::Fn_store3:
      case SimdOperation::Fn_fromFloat64x2Bits:
        MOZ_CRASH("NYI");
    }
    MOZ_CRASH("unexpected opcode");
}

static bool
EmitGrowMemory(FunctionCompiler& f)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    CallCompileState args(f, lineOrBytecode);
    if (!f.startCall(&args))
        return false;

    if (!f.passInstance(&args))
        return false;

    MDefinition* delta;
    if (!f.iter().readGrowMemory(&delta))
        return false;

    if (!f.passArg(delta, ValType::I32, &args))
        return false;

    // As a short-cut, pretend this is an inter-module call so that any pinned
    // heap pointer will be reloaded after the call. This hack will go away once
    // we can stop pinning registers.
    f.finishCall(&args, TlsUsage::CallerSaved);

    MDefinition* ret;
    if (!f.builtinInstanceMethodCall(SymbolicAddress::GrowMemory, args, ValType::I32, &ret))
        return false;

    f.iter().setResult(ret);
    return true;
}

static bool
EmitCurrentMemory(FunctionCompiler& f)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode();

    CallCompileState args(f, lineOrBytecode);

    if (!f.iter().readCurrentMemory())
        return false;

    if (!f.startCall(&args))
        return false;

    if (!f.passInstance(&args))
        return false;

    f.finishCall(&args, TlsUsage::Unused);

    MDefinition* ret;
    if (!f.builtinInstanceMethodCall(SymbolicAddress::CurrentMemory, args, ValType::I32, &ret))
        return false;

    f.iter().setResult(ret);
    return true;
}

static bool
EmitExpr(FunctionCompiler& f)
{
    if (!f.mirGen().ensureBallast())
        return false;

    uint16_t u16;
    MOZ_ALWAYS_TRUE(f.iter().readOp(&u16));
    Op op = Op(u16);

    switch (op) {
      // Control opcodes
      case Op::Nop:
        return f.iter().readNop();
      case Op::Drop:
        return f.iter().readDrop();
      case Op::Block:
        return EmitBlock(f);
      case Op::Loop:
        return EmitLoop(f);
      case Op::If:
        return EmitIf(f);
      case Op::Else:
        return EmitElse(f);
      case Op::End:
        return EmitEnd(f);
      case Op::Br:
        return EmitBr(f);
      case Op::BrIf:
        return EmitBrIf(f);
      case Op::BrTable:
        return EmitBrTable(f);
      case Op::Return:
        return EmitReturn(f);
      case Op::Unreachable:
        if (!f.iter().readUnreachable())
            return false;
        f.unreachableTrap();
        return true;

      // Calls
      case Op::Call:
        return EmitCall(f);
      case Op::CallIndirect:
        return EmitCallIndirect(f, /* oldStyle = */ false);
      case Op::OldCallIndirect:
        return EmitCallIndirect(f, /* oldStyle = */ true);

      // Locals and globals
      case Op::GetLocal:
        return EmitGetLocal(f);
      case Op::SetLocal:
        return EmitSetLocal(f);
      case Op::TeeLocal:
        return EmitTeeLocal(f);
      case Op::GetGlobal:
        return EmitGetGlobal(f);
      case Op::SetGlobal:
        return EmitSetGlobal(f);
      case Op::TeeGlobal:
        return EmitTeeGlobal(f);

      // Select
      case Op::Select:
        return EmitSelect(f);

      // I32
      case Op::I32Const: {
        int32_t i32;
        if (!f.iter().readI32Const(&i32))
            return false;

        f.iter().setResult(f.constant(Int32Value(i32), MIRType::Int32));
        return true;
      }
      case Op::I32Add:
        return EmitAdd(f, ValType::I32, MIRType::Int32);
      case Op::I32Sub:
        return EmitSub(f, ValType::I32, MIRType::Int32);
      case Op::I32Mul:
        return EmitMul(f, ValType::I32, MIRType::Int32);
      case Op::I32DivS:
      case Op::I32DivU:
        return EmitDiv(f, ValType::I32, MIRType::Int32, op == Op::I32DivU);
      case Op::I32RemS:
      case Op::I32RemU:
        return EmitRem(f, ValType::I32, MIRType::Int32, op == Op::I32RemU);
      case Op::I32Min:
      case Op::I32Max:
        return EmitMinMax(f, ValType::I32, MIRType::Int32, op == Op::I32Max);
      case Op::I32Eqz:
        return EmitConversion<MNot>(f, ValType::I32, ValType::I32);
      case Op::I32TruncSF32:
      case Op::I32TruncUF32:
        return EmitTruncate(f, ValType::F32, ValType::I32, op == Op::I32TruncUF32);
      case Op::I32TruncSF64:
      case Op::I32TruncUF64:
        return EmitTruncate(f, ValType::F64, ValType::I32, op == Op::I32TruncUF64);
      case Op::I32WrapI64:
        return EmitConversion<MWrapInt64ToInt32>(f, ValType::I64, ValType::I32);
      case Op::I32ReinterpretF32:
        return EmitReinterpret(f, ValType::I32, ValType::F32, MIRType::Int32);
      case Op::I32Clz:
        return EmitUnaryWithType<MClz>(f, ValType::I32, MIRType::Int32);
      case Op::I32Ctz:
        return EmitUnaryWithType<MCtz>(f, ValType::I32, MIRType::Int32);
      case Op::I32Popcnt:
        return EmitUnaryWithType<MPopcnt>(f, ValType::I32, MIRType::Int32);
      case Op::I32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::I32, MIRType::Int32);
      case Op::I32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::I32, MIRType::Int32);
      case Op::I32Or:
        return EmitBitwise<MBitOr>(f, ValType::I32, MIRType::Int32);
      case Op::I32And:
        return EmitBitwise<MBitAnd>(f, ValType::I32, MIRType::Int32);
      case Op::I32Xor:
        return EmitBitwise<MBitXor>(f, ValType::I32, MIRType::Int32);
      case Op::I32Shl:
        return EmitBitwise<MLsh>(f, ValType::I32, MIRType::Int32);
      case Op::I32ShrS:
        return EmitBitwise<MRsh>(f, ValType::I32, MIRType::Int32);
      case Op::I32ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I32, MIRType::Int32);
      case Op::I32BitNot:
        return EmitBitNot(f, ValType::I32);
      case Op::I32Load8S:
        return EmitLoad(f, ValType::I32, Scalar::Int8);
      case Op::I32Load8U:
        return EmitLoad(f, ValType::I32, Scalar::Uint8);
      case Op::I32Load16S:
        return EmitLoad(f, ValType::I32, Scalar::Int16);
      case Op::I32Load16U:
        return EmitLoad(f, ValType::I32, Scalar::Uint16);
      case Op::I32Load:
        return EmitLoad(f, ValType::I32, Scalar::Int32);
      case Op::I32Store8:
        return EmitStore(f, ValType::I32, Scalar::Int8);
      case Op::I32TeeStore8:
        return EmitTeeStore(f, ValType::I32, Scalar::Int8);
      case Op::I32Store16:
        return EmitStore(f, ValType::I32, Scalar::Int16);
      case Op::I32TeeStore16:
        return EmitTeeStore(f, ValType::I32, Scalar::Int16);
      case Op::I32Store:
        return EmitStore(f, ValType::I32, Scalar::Int32);
      case Op::I32TeeStore:
        return EmitTeeStore(f, ValType::I32, Scalar::Int32);
      case Op::I32Rotr:
      case Op::I32Rotl:
        return EmitRotate(f, ValType::I32, op == Op::I32Rotl);

      // I64
      case Op::I64Const: {
        int64_t i64;
        if (!f.iter().readI64Const(&i64))
            return false;

        f.iter().setResult(f.constant(i64));
        return true;
      }
      case Op::I64Add:
        return EmitAdd(f, ValType::I64, MIRType::Int64);
      case Op::I64Sub:
        return EmitSub(f, ValType::I64, MIRType::Int64);
      case Op::I64Mul:
        return EmitMul(f, ValType::I64, MIRType::Int64);
      case Op::I64DivS:
      case Op::I64DivU:
        return EmitDiv(f, ValType::I64, MIRType::Int64, op == Op::I64DivU);
      case Op::I64RemS:
      case Op::I64RemU:
        return EmitRem(f, ValType::I64, MIRType::Int64, op == Op::I64RemU);
      case Op::I64TruncSF32:
      case Op::I64TruncUF32:
        return EmitTruncate(f, ValType::F32, ValType::I64, op == Op::I64TruncUF32);
      case Op::I64TruncSF64:
      case Op::I64TruncUF64:
        return EmitTruncate(f, ValType::F64, ValType::I64, op == Op::I64TruncUF64);
      case Op::I64ExtendSI32:
      case Op::I64ExtendUI32:
        return EmitExtendI32(f, op == Op::I64ExtendUI32);
      case Op::I64ReinterpretF64:
        return EmitReinterpret(f, ValType::I64, ValType::F64, MIRType::Int64);
      case Op::I64Or:
        return EmitBitwise<MBitOr>(f, ValType::I64, MIRType::Int64);
      case Op::I64And:
        return EmitBitwise<MBitAnd>(f, ValType::I64, MIRType::Int64);
      case Op::I64Xor:
        return EmitBitwise<MBitXor>(f, ValType::I64, MIRType::Int64);
      case Op::I64Shl:
        return EmitBitwise<MLsh>(f, ValType::I64, MIRType::Int64);
      case Op::I64ShrS:
        return EmitBitwise<MRsh>(f, ValType::I64, MIRType::Int64);
      case Op::I64ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I64, MIRType::Int64);
      case Op::I64Rotr:
      case Op::I64Rotl:
        return EmitRotate(f, ValType::I64, op == Op::I64Rotl);
      case Op::I64Eqz:
        return EmitConversion<MNot>(f, ValType::I64, ValType::I32);
      case Op::I64Clz:
        return EmitUnaryWithType<MClz>(f, ValType::I64, MIRType::Int64);
      case Op::I64Ctz:
        return EmitUnaryWithType<MCtz>(f, ValType::I64, MIRType::Int64);
      case Op::I64Popcnt:
        return EmitUnaryWithType<MPopcnt>(f, ValType::I64, MIRType::Int64);
      case Op::I64Load8S:
        return EmitLoad(f, ValType::I64, Scalar::Int8);
      case Op::I64Load8U:
        return EmitLoad(f, ValType::I64, Scalar::Uint8);
      case Op::I64Load16S:
        return EmitLoad(f, ValType::I64, Scalar::Int16);
      case Op::I64Load16U:
        return EmitLoad(f, ValType::I64, Scalar::Uint16);
      case Op::I64Load32S:
        return EmitLoad(f, ValType::I64, Scalar::Int32);
      case Op::I64Load32U:
        return EmitLoad(f, ValType::I64, Scalar::Uint32);
      case Op::I64Load:
        return EmitLoad(f, ValType::I64, Scalar::Int64);
      case Op::I64Store8:
        return EmitStore(f, ValType::I64, Scalar::Int8);
      case Op::I64TeeStore8:
        return EmitTeeStore(f, ValType::I64, Scalar::Int8);
      case Op::I64Store16:
        return EmitStore(f, ValType::I64, Scalar::Int16);
      case Op::I64TeeStore16:
        return EmitTeeStore(f, ValType::I64, Scalar::Int16);
      case Op::I64Store32:
        return EmitStore(f, ValType::I64, Scalar::Int32);
      case Op::I64TeeStore32:
        return EmitTeeStore(f, ValType::I64, Scalar::Int32);
      case Op::I64Store:
        return EmitStore(f, ValType::I64, Scalar::Int64);
      case Op::I64TeeStore:
        return EmitTeeStore(f, ValType::I64, Scalar::Int64);

      // F32
      case Op::F32Const: {
        float f32;
        if (!f.iter().readF32Const(&f32))
            return false;

        f.iter().setResult(f.constant(f32));
        return true;
      }
      case Op::F32Add:
        return EmitAdd(f, ValType::F32, MIRType::Float32);
      case Op::F32Sub:
        return EmitSub(f, ValType::F32, MIRType::Float32);
      case Op::F32Mul:
        return EmitMul(f, ValType::F32, MIRType::Float32);
      case Op::F32Div:
        return EmitDiv(f, ValType::F32, MIRType::Float32, /* isUnsigned = */ false);
      case Op::F32Min:
      case Op::F32Max:
        return EmitMinMax(f, ValType::F32, MIRType::Float32, op == Op::F32Max);
      case Op::F32CopySign:
        return EmitCopySign(f, ValType::F32);
      case Op::F32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F32, MIRType::Float32);
      case Op::F32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F32, MIRType::Float32);
      case Op::F32Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F32, MIRType::Float32);
      case Op::F32Ceil:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::CeilF, ValType::F32);
      case Op::F32Floor:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::FloorF, ValType::F32);
      case Op::F32Trunc:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::TruncF, ValType::F32);
      case Op::F32Nearest:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::NearbyIntF, ValType::F32);
      case Op::F32DemoteF64:
        return EmitConversion<MToFloat32>(f, ValType::F64, ValType::F32);
      case Op::F32ConvertSI32:
        return EmitConversion<MToFloat32>(f, ValType::I32, ValType::F32);
      case Op::F32ConvertUI32:
        return EmitConversion<MWasmUnsignedToFloat32>(f, ValType::I32, ValType::F32);
      case Op::F32ConvertSI64:
      case Op::F32ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F32, MIRType::Float32,
                                             op == Op::F32ConvertUI64);
      case Op::F32ReinterpretI32:
        return EmitReinterpret(f, ValType::F32, ValType::I32, MIRType::Float32);

      case Op::F32Load:
        return EmitLoad(f, ValType::F32, Scalar::Float32);
      case Op::F32Store:
        return EmitStore(f, ValType::F32, Scalar::Float32);
      case Op::F32TeeStore:
        return EmitTeeStore(f, ValType::F32, Scalar::Float32);
      case Op::F32TeeStoreF64:
        return EmitTeeStoreWithCoercion(f, ValType::F32, Scalar::Float64);

      // F64
      case Op::F64Const: {
        double f64;
        if (!f.iter().readF64Const(&f64))
            return false;

        f.iter().setResult(f.constant(f64));
        return true;
      }
      case Op::F64Add:
        return EmitAdd(f, ValType::F64, MIRType::Double);
      case Op::F64Sub:
        return EmitSub(f, ValType::F64, MIRType::Double);
      case Op::F64Mul:
        return EmitMul(f, ValType::F64, MIRType::Double);
      case Op::F64Div:
        return EmitDiv(f, ValType::F64, MIRType::Double, /* isUnsigned = */ false);
      case Op::F64Mod:
        return EmitRem(f, ValType::F64, MIRType::Double, /* isUnsigned = */ false);
      case Op::F64Min:
      case Op::F64Max:
        return EmitMinMax(f, ValType::F64, MIRType::Double, op == Op::F64Max);
      case Op::F64CopySign:
        return EmitCopySign(f, ValType::F64);
      case Op::F64Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F64, MIRType::Double);
      case Op::F64Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F64, MIRType::Double);
      case Op::F64Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F64, MIRType::Double);
      case Op::F64Ceil:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::CeilD, ValType::F64);
      case Op::F64Floor:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::FloorD, ValType::F64);
      case Op::F64Trunc:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::TruncD, ValType::F64);
      case Op::F64Nearest:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::NearbyIntD, ValType::F64);
      case Op::F64Sin:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::SinD, ValType::F64);
      case Op::F64Cos:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::CosD, ValType::F64);
      case Op::F64Tan:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::TanD, ValType::F64);
      case Op::F64Asin:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::ASinD, ValType::F64);
      case Op::F64Acos:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::ACosD, ValType::F64);
      case Op::F64Atan:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::ATanD, ValType::F64);
      case Op::F64Exp:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::ExpD, ValType::F64);
      case Op::F64Log:
        return EmitUnaryMathBuiltinCall(f, SymbolicAddress::LogD, ValType::F64);
      case Op::F64Pow:
        return EmitBinaryMathBuiltinCall(f, SymbolicAddress::PowD, ValType::F64);
      case Op::F64Atan2:
        return EmitBinaryMathBuiltinCall(f, SymbolicAddress::ATan2D, ValType::F64);
      case Op::F64PromoteF32:
        return EmitConversion<MToDouble>(f, ValType::F32, ValType::F64);
      case Op::F64ConvertSI32:
        return EmitConversion<MToDouble>(f, ValType::I32, ValType::F64);
      case Op::F64ConvertUI32:
        return EmitConversion<MWasmUnsignedToDouble>(f, ValType::I32, ValType::F64);
      case Op::F64ConvertSI64:
      case Op::F64ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F64, MIRType::Double,
                                             op == Op::F64ConvertUI64);
      case Op::F64Load:
        return EmitLoad(f, ValType::F64, Scalar::Float64);
      case Op::F64Store:
        return EmitStore(f, ValType::F64, Scalar::Float64);
      case Op::F64TeeStore:
        return EmitTeeStore(f, ValType::F64, Scalar::Float64);
      case Op::F64TeeStoreF32:
        return EmitTeeStoreWithCoercion(f, ValType::F64, Scalar::Float32);
      case Op::F64ReinterpretI64:
        return EmitReinterpret(f, ValType::F64, ValType::I64, MIRType::Double);

      // Comparisons
      case Op::I32Eq:
        return EmitComparison(f, ValType::I32, JSOP_EQ, MCompare::Compare_Int32);
      case Op::I32Ne:
        return EmitComparison(f, ValType::I32, JSOP_NE, MCompare::Compare_Int32);
      case Op::I32LtS:
        return EmitComparison(f, ValType::I32, JSOP_LT, MCompare::Compare_Int32);
      case Op::I32LeS:
        return EmitComparison(f, ValType::I32, JSOP_LE, MCompare::Compare_Int32);
      case Op::I32GtS:
        return EmitComparison(f, ValType::I32, JSOP_GT, MCompare::Compare_Int32);
      case Op::I32GeS:
        return EmitComparison(f, ValType::I32, JSOP_GE, MCompare::Compare_Int32);
      case Op::I32LtU:
        return EmitComparison(f, ValType::I32, JSOP_LT, MCompare::Compare_UInt32);
      case Op::I32LeU:
        return EmitComparison(f, ValType::I32, JSOP_LE, MCompare::Compare_UInt32);
      case Op::I32GtU:
        return EmitComparison(f, ValType::I32, JSOP_GT, MCompare::Compare_UInt32);
      case Op::I32GeU:
        return EmitComparison(f, ValType::I32, JSOP_GE, MCompare::Compare_UInt32);
      case Op::I64Eq:
        return EmitComparison(f, ValType::I64, JSOP_EQ, MCompare::Compare_Int64);
      case Op::I64Ne:
        return EmitComparison(f, ValType::I64, JSOP_NE, MCompare::Compare_Int64);
      case Op::I64LtS:
        return EmitComparison(f, ValType::I64, JSOP_LT, MCompare::Compare_Int64);
      case Op::I64LeS:
        return EmitComparison(f, ValType::I64, JSOP_LE, MCompare::Compare_Int64);
      case Op::I64GtS:
        return EmitComparison(f, ValType::I64, JSOP_GT, MCompare::Compare_Int64);
      case Op::I64GeS:
        return EmitComparison(f, ValType::I64, JSOP_GE, MCompare::Compare_Int64);
      case Op::I64LtU:
        return EmitComparison(f, ValType::I64, JSOP_LT, MCompare::Compare_UInt64);
      case Op::I64LeU:
        return EmitComparison(f, ValType::I64, JSOP_LE, MCompare::Compare_UInt64);
      case Op::I64GtU:
        return EmitComparison(f, ValType::I64, JSOP_GT, MCompare::Compare_UInt64);
      case Op::I64GeU:
        return EmitComparison(f, ValType::I64, JSOP_GE, MCompare::Compare_UInt64);
      case Op::F32Eq:
        return EmitComparison(f, ValType::F32, JSOP_EQ, MCompare::Compare_Float32);
      case Op::F32Ne:
        return EmitComparison(f, ValType::F32, JSOP_NE, MCompare::Compare_Float32);
      case Op::F32Lt:
        return EmitComparison(f, ValType::F32, JSOP_LT, MCompare::Compare_Float32);
      case Op::F32Le:
        return EmitComparison(f, ValType::F32, JSOP_LE, MCompare::Compare_Float32);
      case Op::F32Gt:
        return EmitComparison(f, ValType::F32, JSOP_GT, MCompare::Compare_Float32);
      case Op::F32Ge:
        return EmitComparison(f, ValType::F32, JSOP_GE, MCompare::Compare_Float32);
      case Op::F64Eq:
        return EmitComparison(f, ValType::F64, JSOP_EQ, MCompare::Compare_Double);
      case Op::F64Ne:
        return EmitComparison(f, ValType::F64, JSOP_NE, MCompare::Compare_Double);
      case Op::F64Lt:
        return EmitComparison(f, ValType::F64, JSOP_LT, MCompare::Compare_Double);
      case Op::F64Le:
        return EmitComparison(f, ValType::F64, JSOP_LE, MCompare::Compare_Double);
      case Op::F64Gt:
        return EmitComparison(f, ValType::F64, JSOP_GT, MCompare::Compare_Double);
      case Op::F64Ge:
        return EmitComparison(f, ValType::F64, JSOP_GE, MCompare::Compare_Double);

      // SIMD
#define CASE(TYPE, OP, SIGN)                                                    \
      case Op::TYPE##OP:                                                      \
        return EmitSimdOp(f, ValType::TYPE, SimdOperation::Fn_##OP, SIGN);
#define I8x16CASE(OP) CASE(I8x16, OP, SimdSign::Signed)
#define I16x8CASE(OP) CASE(I16x8, OP, SimdSign::Signed)
#define I32x4CASE(OP) CASE(I32x4, OP, SimdSign::Signed)
#define F32x4CASE(OP) CASE(F32x4, OP, SimdSign::NotApplicable)
#define B8x16CASE(OP) CASE(B8x16, OP, SimdSign::NotApplicable)
#define B16x8CASE(OP) CASE(B16x8, OP, SimdSign::NotApplicable)
#define B32x4CASE(OP) CASE(B32x4, OP, SimdSign::NotApplicable)
#define ENUMERATE(TYPE, FORALL, DO)                                             \
      case Op::TYPE##Constructor:                                             \
        return EmitSimdOp(f, ValType::TYPE, SimdOperation::Constructor, SimdSign::NotApplicable); \
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

      case Op::I8x16Const: {
        I8x16 i8x16;
        if (!f.iter().readI8x16Const(&i8x16))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX16(i8x16), MIRType::Int8x16));
        return true;
      }
      case Op::I16x8Const: {
        I16x8 i16x8;
        if (!f.iter().readI16x8Const(&i16x8))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX8(i16x8), MIRType::Int16x8));
        return true;
      }
      case Op::I32x4Const: {
        I32x4 i32x4;
        if (!f.iter().readI32x4Const(&i32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(i32x4), MIRType::Int32x4));
        return true;
      }
      case Op::F32x4Const: {
        F32x4 f32x4;
        if (!f.iter().readF32x4Const(&f32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(f32x4), MIRType::Float32x4));
        return true;
      }
      case Op::B8x16Const: {
        I8x16 i8x16;
        if (!f.iter().readB8x16Const(&i8x16))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX16(i8x16), MIRType::Bool8x16));
        return true;
      }
      case Op::B16x8Const: {
        I16x8 i16x8;
        if (!f.iter().readB16x8Const(&i16x8))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX8(i16x8), MIRType::Bool16x8));
        return true;
      }
      case Op::B32x4Const: {
        I32x4 i32x4;
        if (!f.iter().readB32x4Const(&i32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(i32x4), MIRType::Bool32x4));
        return true;
      }

      // SIMD unsigned integer operations.
      case Op::I8x16addSaturateU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_addSaturate, SimdSign::Unsigned);
      case Op::I8x16subSaturateU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_subSaturate, SimdSign::Unsigned);
      case Op::I8x16shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Op::I8x16lessThanU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Op::I8x16lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Op::I8x16greaterThanU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Op::I8x16greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Op::I8x16extractLaneU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_extractLane, SimdSign::Unsigned);

      case Op::I16x8addSaturateU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_addSaturate, SimdSign::Unsigned);
      case Op::I16x8subSaturateU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_subSaturate, SimdSign::Unsigned);
      case Op::I16x8shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Op::I16x8lessThanU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Op::I16x8lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Op::I16x8greaterThanU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Op::I16x8greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Op::I16x8extractLaneU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_extractLane, SimdSign::Unsigned);

      case Op::I32x4shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Op::I32x4lessThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Op::I32x4lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Op::I32x4greaterThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Op::I32x4greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Op::I32x4fromFloat32x4U:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_fromFloat32x4, SimdSign::Unsigned);

      // Atomics
      case Op::I32AtomicsLoad:
        return EmitAtomicsLoad(f);
      case Op::I32AtomicsStore:
        return EmitAtomicsStore(f);
      case Op::I32AtomicsBinOp:
        return EmitAtomicsBinOp(f);
      case Op::I32AtomicsCompareExchange:
        return EmitAtomicsCompareExchange(f);
      case Op::I32AtomicsExchange:
        return EmitAtomicsExchange(f);
      // Memory Operators
      case Op::GrowMemory:
        return EmitGrowMemory(f);
      case Op::CurrentMemory:
        return EmitCurrentMemory(f);
      case Op::Limit:;
    }

    MOZ_CRASH("unexpected wasm opcode");
}

bool
wasm::IonCompileFunction(CompileTask* task, FuncCompileUnit* unit)
{
    MOZ_ASSERT(unit->mode() == CompileMode::Ion);

    const FuncBytes& func = unit->func();
    const ModuleEnvironment& env = task->env();

    Decoder d(func.bytes());

    // Build the local types vector.

    ValTypeVector locals;
    if (!locals.appendAll(func.sig().args()))
        return false;
    if (!DecodeLocalEntries(d, env.kind, &locals))
        return false;

    // Set up for Ion compilation.

    JitContext jitContext(&task->alloc());
    const JitCompileOptions options;
    MIRGraph graph(&task->alloc());
    CompileInfo compileInfo(locals.length());
    MIRGenerator mir(nullptr, options, &task->alloc(), &graph, &compileInfo,
                     IonOptimizations.get(OptimizationLevel::Wasm));
    mir.initMinWasmHeapLength(env.minMemoryLength);

    // Capture the prologue's trap site before decoding the function.

    TrapOffset prologueTrapOffset;

    // Build MIR graph
    {
        FunctionCompiler f(env, d, func, locals, mir);
        if (!f.init())
            return false;

        prologueTrapOffset = f.iter().trapOffset();

        if (!f.startBlock())
            return false;

        if (!f.iter().readFunctionStart(f.sig().ret()))
            return false;

        while (!f.done()) {
            if (!EmitExpr(f))
                return false;
        }

        if (f.inDeadCode() || IsVoid(f.sig().ret()))
            f.returnVoid();
        else
            f.returnExpr(f.iter().getResult());

        if (!f.iter().readFunctionEnd())
            return false;

        f.finish();
    }

    // Compile MIR graph
    {
        jit::SpewBeginFunction(&mir, nullptr);
        jit::AutoSpewEndFunction spewEndFunction(&mir);

        if (!OptimizeMIR(&mir))
            return false;

        LIRGraph* lir = GenerateLIR(&mir);
        if (!lir)
            return false;

        SigIdDesc sigId = env.funcSigs[func.index()]->id;

        CodeGenerator codegen(&mir, lir, &task->masm());

        FuncOffsets offsets;
        if (!codegen.generateWasm(sigId, prologueTrapOffset, &offsets))
            return false;

        unit->finish(offsets);
    }

    return true;
}
