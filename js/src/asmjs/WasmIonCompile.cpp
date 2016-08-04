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

#include "asmjs/WasmIonCompile.h"

#include "asmjs/WasmBaselineCompile.h"
#include "asmjs/WasmBinaryIterator.h"
#include "asmjs/WasmGenerator.h"

#include "jit/CodeGenerator.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::DebugOnly;
using mozilla::Maybe;

namespace {

typedef Vector<MBasicBlock*, 8, SystemAllocPolicy> BlockVector;

struct IonCompilePolicy : ExprIterPolicy
{
    // Producing output is what we're all about here.
    static const bool Output = true;

    // We store SSA definitions in the value stack.
    typedef MDefinition* Value;

    // We store loop headers and then/else blocks in the control flow stack.
    typedef MBasicBlock* ControlItem;
};

typedef ExprIter<IonCompilePolicy> IonExprIter;

class FunctionCompiler;

// CallCompileState describes a call that is being compiled. Due to expression
// nesting, multiple calls can be in the middle of compilation at the same time
// and these are tracked in a stack by FunctionCompiler.

enum class PassTls { False = false, True = true };

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

    // Accumulates the register arguments while compiling arguments.
    MWasmCall::Args regArgs_;

    // Accumulates the stack arguments while compiling arguments. This is only
    // necessary to track when childClobbers_ is true so that the stack offsets
    // can be updated.
    Vector<MAsmJSPassStackArg*, 0, SystemAllocPolicy> stackArgs_;

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

    const ModuleGeneratorData& mg_;
    IonExprIter                iter_;
    const FuncBytes&           func_;
    const ValTypeVector&       locals_;
    size_t                     lastReadCallSite_;

    TempAllocator&             alloc_;
    MIRGraph&                  graph_;
    const CompileInfo&         info_;
    MIRGenerator&              mirGen_;

    MInstruction*              dummyIns_;

    MBasicBlock*               curBlock_;
    CallCompileStateVector     callStack_;
    uint32_t                   maxStackArgBytes_;

    uint32_t                   loopDepth_;
    uint32_t                   blockDepth_;
    ControlFlowPatchsVector    blockPatches_;

    FuncCompileResults&        compileResults_;

    // TLS pointer argument to the current function.
    MAsmJSParameter*           tlsPointer_;

  public:
    FunctionCompiler(const ModuleGeneratorData& mg,
                     Decoder& decoder,
                     const FuncBytes& func,
                     const ValTypeVector& locals,
                     MIRGenerator& mirGen,
                     FuncCompileResults& compileResults)
      : mg_(mg),
        iter_(decoder),
        func_(func),
        locals_(locals),
        lastReadCallSite_(0),
        alloc_(mirGen.alloc()),
        graph_(mirGen.graph()),
        info_(mirGen.info()),
        mirGen_(mirGen),
        dummyIns_(nullptr),
        curBlock_(nullptr),
        maxStackArgBytes_(0),
        loopDepth_(0),
        blockDepth_(0),
        compileResults_(compileResults),
        tlsPointer_(nullptr)
    {}

    const ModuleGeneratorData& mg() const    { return mg_; }
    IonExprIter&               iter()        { return iter_; }
    TempAllocator&             alloc() const { return alloc_; }
    MacroAssembler&            masm() const  { return compileResults_.masm(); }
    const Sig&                 sig() const   { return func_.sig(); }

    bool init()
    {
        // Prepare the entry block for MIR generation:

        const ValTypeVector& args = func_.sig().args();

        if (!mirGen_.ensureBallast())
            return false;
        if (!newBlock(/* prev */ nullptr, &curBlock_))
            return false;

        for (ABIArgValTypeIter i(args); !i.done(); i++) {
            MAsmJSParameter* ins = MAsmJSParameter::New(alloc(), *i, i.mirType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i.index()), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        // Set up a parameter that receives the hidden TLS pointer argument.
        tlsPointer_ = MAsmJSParameter::New(alloc(), ABIArg(WasmTlsReg), MIRType::Pointer);
        curBlock_->add(tlsPointer_);
        if (!mirGen_.ensureBallast())
            return false;

        for (size_t i = args.length(); i < locals_.length(); i++) {
            MInstruction* ins = nullptr;
            switch (locals_[i]) {
              case ValType::I32:
                ins = MConstant::NewAsmJS(alloc(), Int32Value(0), MIRType::Int32);
                break;
              case ValType::I64:
                ins = MConstant::NewInt64(alloc(), 0);
                break;
              case ValType::F32:
                ins = MConstant::NewAsmJS(alloc(), Float32Value(0.f), MIRType::Float32);
                break;
              case ValType::F64:
                ins = MConstant::NewAsmJS(alloc(), DoubleValue(0.0), MIRType::Double);
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
              case ValType::Limit:
                MOZ_CRASH("Limit");
            }

            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        dummyIns_ = MConstant::NewAsmJS(alloc(), Int32Value(0), MIRType::Int32);
        curBlock_->add(dummyIns_);

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

    MDefinition* constant(Value v, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        MConstant* constant = MConstant::NewAsmJS(alloc(), v, type);
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

    // Specialized for MToFloat32
    template <class T>
    MDefinition* unary(MDefinition* op)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* unary(MDefinition* op, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), op, type);
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
        T* ins = T::NewAsmJS(alloc(), lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    bool mustPreserveNaN(MIRType type)
    {
        return IsFloatingPointType(type) && mg().kind == ModuleKind::Wasm;
    }

    MDefinition* sub(MDefinition* lhs, MDefinition* rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        // wasm can't fold x - 0.0 because of NaN with custom payloads.
        MSub* ins = MSub::NewAsmJS(alloc(), lhs, rhs, type, mustPreserveNaN(type));
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
        return MSimdConvert::AddLegalized(alloc(), curBlock_, vec, to, sign);
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

    MDefinition* div(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd,
                     bool trapOnError)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MDiv::NewAsmJS(alloc(), lhs, rhs, type, unsignd, trapOnError,
                                   mustPreserveNaN(type));
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* mod(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd,
                     bool trapOnError)
    {
        if (inDeadCode())
            return nullptr;
        MMod* ins = MMod::NewAsmJS(alloc(), lhs, rhs, type, unsignd, trapOnError);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* bitwise(MDefinition* lhs, MDefinition* rhs, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* bitwise(MDefinition* op)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* select(MDefinition* trueExpr, MDefinition* falseExpr, MDefinition* condExpr)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MAsmSelect::New(alloc(), trueExpr, falseExpr, condExpr);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* extendI32(MDefinition* op, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        MExtendInt32ToInt64* ins = MExtendInt32ToInt64::NewAsmJS(alloc(), op, isUnsigned);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* convertI64ToFloatingPoint(MDefinition* op, MIRType type, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        MInt64ToFloatingPoint* ins = MInt64ToFloatingPoint::NewAsmJS(alloc(), op, type, isUnsigned);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* rotate(MDefinition* input, MDefinition* count, MIRType type, bool left)
    {
        if (inDeadCode())
            return nullptr;
        auto* ins = MRotate::NewAsmJS(alloc(), input, count, type, left);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* truncate(MDefinition* op, bool isUnsigned)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), op, isUnsigned);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* compare(MDefinition* lhs, MDefinition* rhs, JSOp op, MCompare::CompareType type)
    {
        if (inDeadCode())
            return nullptr;
        MCompare* ins = MCompare::NewAsmJS(alloc(), lhs, rhs, op, type);
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
    MDefinition* loadHeapPrivate(MDefinition* base, const MWasmMemoryAccess& access,
                                 bool isInt64 = false)
    {
        if (inDeadCode())
            return nullptr;

        MInstruction* load = nullptr;
        if (mg().isAsmJS()) {
            load = MAsmJSLoadHeap::New(alloc(), base, access);
        } else {
            if (!mg().usesSignal.forOOB)
                curBlock_->add(MWasmBoundsCheck::New(alloc(), base, access));
            load = MWasmLoad::New(alloc(), base, access, isInt64);
        }

        curBlock_->add(load);
        return load;
    }

    void storeHeapPrivate(MDefinition* base, const MWasmMemoryAccess& access, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MInstruction* store = nullptr;
        if (mg().isAsmJS()) {
            store = MAsmJSStoreHeap::New(alloc(), base, access, v);
        } else {
            if (!mg().usesSignal.forOOB)
                curBlock_->add(MWasmBoundsCheck::New(alloc(), base, access));
            store = MWasmStore::New(alloc(), base, access, v);
        }

        curBlock_->add(store);
    }

  public:
    MDefinition* loadHeap(MDefinition* base, const MWasmMemoryAccess& access, bool isInt64)
    {
        MOZ_ASSERT(!Scalar::isSimdType(access.accessType()), "SIMD loads should use loadSimdHeap");
        return loadHeapPrivate(base, access, isInt64);
    }
    MDefinition* loadSimdHeap(MDefinition* base, const MWasmMemoryAccess& access)
    {
        MOZ_ASSERT(Scalar::isSimdType(access.accessType()), "non-SIMD loads should use loadHeap");
        return loadHeapPrivate(base, access);
    }
    MDefinition* loadAtomicHeap(MDefinition* base, const MWasmMemoryAccess& access)
    {
        return loadHeapPrivate(base, access);
    }

    void storeHeap(MDefinition* base, const MWasmMemoryAccess& access, MDefinition* v)
    {
        MOZ_ASSERT(!Scalar::isSimdType(access.accessType()), "SIMD store should use storeSimdHeap");
        storeHeapPrivate(base, access, v);
    }
    void storeSimdHeap(MDefinition* base, const MWasmMemoryAccess& access, MDefinition* v)
    {
        MOZ_ASSERT(Scalar::isSimdType(access.accessType()), "non-SIMD stores should use storeHeap");
        storeHeapPrivate(base, access, v);
    }
    void storeAtomicHeap(MDefinition* base, const MWasmMemoryAccess& access, MDefinition* v)
    {
        storeHeapPrivate(base, access, v);
    }

    MDefinition* atomicCompareExchangeHeap(MDefinition* base, const MWasmMemoryAccess& access,
                                           MDefinition* oldv, MDefinition* newv)
    {
        if (inDeadCode())
            return nullptr;

        auto* cas = MAsmJSCompareExchangeHeap::New(alloc(), base, access, oldv, newv, tlsPointer_);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicExchangeHeap(MDefinition* base, const MWasmMemoryAccess& access,
                                    MDefinition* value)
    {
        if (inDeadCode())
            return nullptr;

        auto* cas = MAsmJSAtomicExchangeHeap::New(alloc(), base, access, value, tlsPointer_);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicBinopHeap(js::jit::AtomicOp op,
                                 MDefinition* base, const MWasmMemoryAccess& access,
                                 MDefinition* v)
    {
        if (inDeadCode())
            return nullptr;

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
        if (mg_.usesSignal.forInterrupt)
            return;

        if (inDeadCode())
            return;

        // WasmHandleExecutionInterrupt takes 0 arguments and the stack is
        // always ABIStackAlignment-aligned, but don't forget to account for
        // ShadowStackSpace and any other ABI warts.
        ABIArgGenerator abi;

        propagateMaxStackArgBytes(abi.stackBytesConsumedSoFar());

        CallSiteDesc callDesc(0, CallSiteDesc::Relative);
        curBlock_->add(MAsmJSInterruptCheck::New(alloc()));
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
    // MIRGenerator::maxAsmJSStackArgBytes.) Naively, this would just be the
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

    bool passArg(MDefinition* argDef, ValType type, CallCompileState* call)
    {
        if (inDeadCode())
            return true;

        ABIArg arg = call->abi_.next(ToMIRType(type));
        switch (arg.kind()) {
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR: {
            auto mirLow = MWrapInt64ToInt32::NewAsmJS(alloc(), argDef, /* bottomHalf = */ true);
            curBlock_->add(mirLow);
            auto mirHigh = MWrapInt64ToInt32::NewAsmJS(alloc(), argDef, /* bottomHalf = */ false);
            curBlock_->add(mirHigh);
            return call->regArgs_.append(MWasmCall::Arg(AnyRegister(arg.gpr64().low), mirLow)) &&
                   call->regArgs_.append(MWasmCall::Arg(AnyRegister(arg.gpr64().high), mirHigh));
          }
#endif
          case ABIArg::GPR:
          case ABIArg::FPU:
            return call->regArgs_.append(MWasmCall::Arg(arg.reg(), argDef));
          case ABIArg::Stack: {
            auto* mir = MAsmJSPassStackArg::New(alloc(), arg.offsetFromArgBase(), argDef);
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

    bool finishCall(CallCompileState* call, PassTls passTls)
    {
        MOZ_ALWAYS_TRUE(callStack_.popCopy() == call);

        if (inDeadCode()) {
            propagateMaxStackArgBytes(call->maxChildStackBytes_);
            return true;
        }

        if (passTls == PassTls::True) {
            if (!call->regArgs_.append(MWasmCall::Arg(AnyRegister(WasmTlsReg), tlsPointer_)))
                return false;
        }

        uint32_t stackBytes = call->abi_.stackBytesConsumedSoFar();

        if (call->childClobbers_) {
            call->spIncrement_ = AlignBytes(call->maxChildStackBytes_, AsmJSStackAlignment);
            for (MAsmJSPassStackArg* stackArg : call->stackArgs_)
                stackArg->incrementOffset(call->spIncrement_);
            stackBytes += call->spIncrement_;
        } else {
            call->spIncrement_ = 0;
            stackBytes = Max(stackBytes, call->maxChildStackBytes_);
        }

        propagateMaxStackArgBytes(stackBytes);
        return true;
    }

  private:
    bool callPrivate(MWasmCall::Callee callee, MWasmCall::PreservesTlsReg preservesTlsReg,
                     const CallCompileState& call, ExprType ret, MDefinition** def)
    {
        MOZ_ASSERT(!inDeadCode());

        CallSiteDesc::Kind kind = CallSiteDesc::Kind(-1);
        switch (callee.which()) {
          case MWasmCall::Callee::Internal: kind = CallSiteDesc::Relative; break;
          case MWasmCall::Callee::Import:   kind = CallSiteDesc::Register; break;
          case MWasmCall::Callee::Dynamic:  kind = CallSiteDesc::Register; break;
          case MWasmCall::Callee::Builtin:  kind = CallSiteDesc::Register; break;
        }

        MWasmCall* ins =
          MWasmCall::New(alloc(), CallSiteDesc(call.lineOrBytecode_, kind), callee, call.regArgs_,
                          ToMIRType(ret), call.spIncrement_, preservesTlsReg);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

  public:
    bool internalCall(const Sig& sig, uint32_t funcIndex, const CallCompileState& call,
                      MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        auto callee = MWasmCall::Callee::internal(funcIndex);
        return callPrivate(callee, MWasmCall::PreservesTlsReg::True, call, sig.ret(), def);
    }

    bool funcPtrCall(uint32_t sigIndex, uint32_t length, uint32_t globalDataOffset,
                     MDefinition* index, const CallCompileState& call, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MWasmCall::Callee callee;
        if (mg().isAsmJS()) {
            MOZ_ASSERT(IsPowerOfTwo(length));
            MConstant* mask = MConstant::New(alloc(), Int32Value(length - 1));
            curBlock_->add(mask);
            MBitAnd* maskedIndex = MBitAnd::NewAsmJS(alloc(), index, mask, MIRType::Int32);
            curBlock_->add(maskedIndex);
            MInstruction* ptrFun = MAsmJSLoadFuncPtr::New(alloc(), maskedIndex, globalDataOffset);
            curBlock_->add(ptrFun);
            callee = MWasmCall::Callee(ptrFun);
            MOZ_ASSERT(mg_.sigs[sigIndex].id.kind() == SigIdDesc::Kind::None);
        } else {
            MInstruction* ptrFun = MAsmJSLoadFuncPtr::New(alloc(), index, length, globalDataOffset);
            curBlock_->add(ptrFun);
            callee = MWasmCall::Callee(ptrFun, mg_.sigs[sigIndex].id);
        }

        return callPrivate(callee, MWasmCall::PreservesTlsReg::True, call,
                           mg_.sigs[sigIndex].ret(), def);
    }

    bool ffiCall(unsigned globalDataOffset, const CallCompileState& call, ExprType ret,
                 MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        auto callee = MWasmCall::Callee::import(globalDataOffset);
        return callPrivate(callee, MWasmCall::PreservesTlsReg::False, call, ret, def);
    }

    bool builtinCall(SymbolicAddress builtin, const CallCompileState& call, ValType type,
                     MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        return callPrivate(MWasmCall::Callee(builtin), MWasmCall::PreservesTlsReg::False,
                           call, ToExprType(type), def);
    }

    /*********************************************** Control flow generation */

    inline bool inDeadCode() const {
        return curBlock_ == nullptr;
    }

    void returnExpr(MDefinition* expr)
    {
        if (inDeadCode())
            return;

        MAsmJSReturn* ins = MAsmJSReturn::New(alloc(), expr, tlsPointer_);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void returnVoid()
    {
        if (inDeadCode())
            return;

        MAsmJSVoidReturn* ins = MAsmJSVoidReturn::New(alloc(), tlsPointer_);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void unreachableTrap()
    {
        if (inDeadCode())
            return;

        auto* ins = MAsmThrowUnreachable::New(alloc());
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

    MDefinition* popDefIfPushed(bool shouldReturn = true)
    {
        if (!hasPushed(curBlock_))
            return nullptr;
        MDefinition* def = curBlock_->pop();
        MOZ_ASSERT_IF(def->type() == MIRType::Value, !shouldReturn);
        return shouldReturn ? def : nullptr;
    }

    template <typename GetBlock>
    bool ensurePushInvariants(const GetBlock& getBlock, size_t numBlocks)
    {
        // Preserve the invariant that, for every iterated MBasicBlock, either:
        // every MBasicBlock has a pushed expression with the same type (to
        // prevent creating used phis with type Value) OR no MBasicBlock has any
        // pushed expression. This is required by MBasicBlock::addPredecessor.
        if (numBlocks < 2)
            return true;

        MBasicBlock* block = getBlock(0);

        bool allPushed = hasPushed(block);
        if (allPushed) {
            MIRType type = peekPushedDef(block)->type();
            for (size_t i = 1; allPushed && i < numBlocks; i++) {
                block = getBlock(i);
                allPushed = hasPushed(block) && peekPushedDef(block)->type() == type;
            }
        }

        if (!allPushed) {
            for (size_t i = 0; i < numBlocks; i++) {
                block = getBlock(i);
                if (!hasPushed(block))
                    block->push(dummyIns_);
            }
        }

        return allPushed;
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

            auto getBlock = [&](size_t i) -> MBasicBlock* { return blocks[i]; };
            bool yieldsValue = ensurePushInvariants(getBlock, numJoinPreds);

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
            *def = popDefIfPushed(yieldsValue);
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

        blockDepth_ += 2;
        loopDepth_++;

        if (inDeadCode())
            return true;

        // Create the loop header.
        MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_ - 1);
        *loopHeader = MBasicBlock::NewAsmJS(mirGraph(), info(), curBlock_,
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
        if (!loopEntry->setBackedgeAsmJS(backedge))
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
        MOZ_ASSERT(blockDepth_ >= 2);
        MOZ_ASSERT(loopDepth_);

        uint32_t headerLabel = blockDepth_ - 1;
        uint32_t afterLabel = blockDepth_ - 2;

        if (!loopHeader) {
            MOZ_ASSERT(inDeadCode());
            MOZ_ASSERT(afterLabel >= blockPatches_.length() || blockPatches_[afterLabel].empty());
            MOZ_ASSERT(headerLabel >= blockPatches_.length() || blockPatches_[headerLabel].empty());
            *loopResult = nullptr;
            blockDepth_ -= 2;
            loopDepth_--;
            return true;
        }

        // Expr::Loop doesn't have an implicit backedge so temporarily set
        // aside the end of the loop body to bind backedges.
        MBasicBlock* loopBody = curBlock_;
        curBlock_ = nullptr;

        // TODO (bug 1253544): blocks branching to the top join to a single
        // backedge block. Could they directly be set as backedges of the loop
        // instead?
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
        if (!bindBranches(afterLabel, loopResult))
            return false;

        // If we have not created a new block in bindBranches, we're still on
        // the inner loop body, which loop depth is incorrect.
        if (curBlock_ && curBlock_->loopDepth() != loopDepth_) {
            MBasicBlock* out;
            if (!goToNewBlock(curBlock_, &out))
                return false;
            curBlock_ = out;
        }

        blockDepth_ -= 2;
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

        MGoto* jump = MGoto::NewAsm(alloc());
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

        MTest* test = MTest::NewAsm(alloc(), condition, joinBlock);
        if (!addControlFlowPatch(test, relativeDepth, MTest::TrueBranchIndex))
            return false;

        pushDef(maybeValue);

        curBlock_->end(test);
        curBlock_ = joinBlock;
        return true;
    }

    bool brTable(MDefinition* expr, uint32_t defaultDepth, const Uint32Vector& depths,
                 MDefinition* maybeValue)
    {
        if (inDeadCode())
            return true;

        size_t numCases = depths.length();
        MOZ_ASSERT(numCases <= INT32_MAX);
        MOZ_ASSERT(numCases);

        MTableSwitch* table = MTableSwitch::New(alloc(), expr, 0, int32_t(numCases - 1));

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

    uint32_t readCallSiteLineOrBytecode(uint32_t callOffset) {
        if (!func_.callSiteLineNums().empty())
            return func_.callSiteLineNums()[lastReadCallSite_++];
        return callOffset;
    }

    bool done() const { return iter_.done(); }

    /*************************************************************************/
  private:
    bool newBlock(MBasicBlock* pred, MBasicBlock** block)
    {
        *block = MBasicBlock::NewAsmJS(mirGraph(), info(), pred, MBasicBlock::NORMAL);
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

        auto getBlock = [&](size_t i) -> MBasicBlock* {
            if (i < patches.length())
                return patches[i].ins->block();
            return curBlock_;
        };

        bool yieldsValue = ensurePushInvariants(getBlock, patches.length() + !!curBlock_);

        MBasicBlock* join = nullptr;
        MControlInstruction* ins = patches[0].ins;
        MBasicBlock* pred = ins->block();
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

        *def = popDefIfPushed(yieldsValue);

        patches.clear();
        return true;
    }
};

template <>
MDefinition* FunctionCompiler::unary<MToFloat32>(MDefinition* op)
{
    if (inDeadCode())
        return nullptr;
    auto* ins = MToFloat32::NewAsmJS(alloc(), op, mustPreserveNaN(op->type()));
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

    MBasicBlock *loopHeader;
    if (!f.startLoop(&loopHeader))
        return false;

    f.addInterruptCheck();

    f.iter().controlItem() = loopHeader;
    return true;
}

static bool
EmitIf(FunctionCompiler& f)
{
    MDefinition* condition;
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
    MBasicBlock *block = f.iter().controlItem();

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
    MBasicBlock *block = f.iter().controlItem();

    LabelKind kind;
    ExprType type;
    MDefinition* value;
    if (!f.iter().readEnd(&kind, &type, &value))
        return false;

    if (!IsVoid(type))
        f.pushDef(value);

    MDefinition* def;
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

    f.iter().setResult(def);
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

    uint32_t depth;
    for (size_t i = 0; i < tableLength; ++i) {
        if (!f.iter().readBrTableEntry(type, &depth))
            return false;
        depths.infallibleAppend(depth);
    }

    // Read the default label.
    if (!f.iter().readBrTableEntry(type, &depth))
        return false;

    MDefinition* maybeValue = IsVoid(type) ? nullptr : value;

    if (tableLength == 0)
        return f.br(depth, maybeValue);

    return f.brTable(index, depth, depths, maybeValue);
}

static bool
EmitReturn(FunctionCompiler& f)
{
    MDefinition* value;
    if (!f.iter().readReturn(&value))
        return false;

    if (IsVoid(f.sig().ret())) {
        f.returnVoid();
        return true;
    }

    f.returnExpr(value);
    return true;
}

static bool
EmitCallArgs(FunctionCompiler& f, const Sig& sig, CallCompileState* call)
{
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

    return f.finishCall(call, PassTls::True);
}

static bool
EmitCall(FunctionCompiler& f, uint32_t callOffset)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    uint32_t calleeIndex;
    uint32_t arity;
    if (!f.iter().readCall(&calleeIndex, &arity))
        return false;

    const Sig& sig = *f.mg().funcSigs[calleeIndex];

    CallCompileState call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    if (!f.iter().readCallReturn(sig.ret()))
        return false;

    MDefinition* def;
    if (!f.internalCall(sig, calleeIndex, call, &def))
        return false;

    if (IsVoid(sig.ret()))
        return true;

    f.iter().setResult(def);
    return true;
}

static bool
EmitCallIndirect(FunctionCompiler& f, uint32_t callOffset)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    uint32_t sigIndex;
    uint32_t arity;
    if (!f.iter().readCallIndirect(&sigIndex, &arity))
        return false;

    const Sig& sig = f.mg().sigs[sigIndex];

    CallCompileState call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    MDefinition* callee;
    if (!f.iter().readCallIndirectCallee(&callee))
        return false;

    if (!f.iter().readCallReturn(sig.ret()))
        return false;

    const TableDesc& table = f.mg().isAsmJS()
                             ? f.mg().tables[f.mg().asmJSSigToTableIndex[sigIndex]]
                             : f.mg().tables[0];

    MDefinition* def;
    if (!f.funcPtrCall(sigIndex, table.initial, table.globalDataOffset, callee, call, &def))
        return false;

    if (IsVoid(sig.ret()))
        return true;

    f.iter().setResult(def);
    return true;
}

static bool
EmitCallImport(FunctionCompiler& f, uint32_t callOffset)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    uint32_t funcImportIndex;
    uint32_t arity;
    if (!f.iter().readCallImport(&funcImportIndex, &arity))
        return false;

    const FuncImportGenDesc& funcImport = f.mg().funcImports[funcImportIndex];
    const Sig& sig = *funcImport.sig;

    CallCompileState call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    if (!f.iter().readCallReturn(sig.ret()))
        return false;

    MDefinition* def;
    if (!f.ffiCall(funcImport.globalDataOffset, call, sig.ret(), &def))
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
EmitGetGlobal(FunctionCompiler& f)
{
    uint32_t id;
    if (!f.iter().readGetGlobal(f.mg().globals, &id))
        return false;

    const GlobalDesc& global = f.mg().globals[id];
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
        result = f.constant(Float32Value(value.f32()), mirType);
        break;
      case ValType::F64:
        result = f.constant(DoubleValue(value.f64()), mirType);
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
    if (!f.iter().readSetGlobal(f.mg().globals, &id, &value))
        return false;

    const GlobalDesc& global = f.mg().globals[id];
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
        if (f.mg().isAsmJS())
            f.iter().setResult(f.truncate<MTruncateToInt32>(input, isUnsigned));
        else
            f.iter().setResult(f.truncate<MWasmTruncateToInt32>(input, isUnsigned));
    } else {
        MOZ_ASSERT(resultType == ValType::I64);
        MOZ_ASSERT(!f.mg().isAsmJS());
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

    f.iter().setResult(f.unary<MAsmReinterpret>(input, mirType));
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

template <typename MIRClass>
static bool
EmitBitwise(FunctionCompiler& f, ValType operandType)
{
    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    f.iter().setResult(f.bitwise<MIRClass>(input));
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

    f.iter().setResult(f.bitwise<MIRClass>(lhs, rhs, mirType));
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

    bool trapOnError = !f.mg().isAsmJS();
    f.iter().setResult(f.div(lhs, rhs, mirType, isUnsigned, trapOnError));
    return true;
}

static bool
EmitRem(FunctionCompiler& f, ValType operandType, MIRType mirType, bool isUnsigned)
{
    MDefinition* lhs;
    MDefinition* rhs;
    if (!f.iter().readBinary(operandType, &lhs, &rhs))
        return false;

    bool trapOnError = !f.mg().isAsmJS();
    f.iter().setResult(f.mod(lhs, rhs, mirType, isUnsigned, trapOnError));
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
    ExprType type;
    MDefinition* trueValue;
    MDefinition* falseValue;
    MDefinition* condition;
    if (!f.iter().readSelect(&type, &trueValue, &falseValue, &condition))
        return false;

    if (!IsVoid(type))
        f.iter().setResult(f.select(trueValue, falseValue, condition));

    return true;
}

static bool
EmitLoad(FunctionCompiler& f, ValType type, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    if (!f.iter().readLoad(type, Scalar::byteSize(viewType), &addr))
        return false;

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
    f.iter().setResult(f.loadHeap(addr.base, access, type == ValType::I64));
    return true;
}

static bool
EmitStore(FunctionCompiler& f, ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
    f.storeHeap(addr.base, access, value);
    return true;
}

static bool
EmitStoreWithCoercion(FunctionCompiler& f, ValType resultType, Scalar::Type viewType)
{
    LinearMemoryAddress<MDefinition*> addr;
    MDefinition* value;
    if (!f.iter().readStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    if (resultType == ValType::F32 && viewType == Scalar::Float64)
        value = f.unary<MToDouble>(value);
    else if (resultType == ValType::F64 && viewType == Scalar::Float32)
        value = f.unary<MToFloat32>(value);
    else
        MOZ_CRASH("unexpected coerced store");

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
    f.storeHeap(addr.base, access, value);
    return true;
}

static bool
EmitUnaryMathBuiltinCall(FunctionCompiler& f, uint32_t callOffset, SymbolicAddress callee,
                         ValType operandType)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    CallCompileState call(f, lineOrBytecode);
    if (!f.startCall(&call))
        return false;

    MDefinition* input;
    if (!f.iter().readUnary(operandType, &input))
        return false;

    if (!f.passArg(input, operandType, &call))
        return false;

    if (!f.finishCall(&call, PassTls::False))
        return false;

    MDefinition* def;
    if (!f.builtinCall(callee, call, operandType, &def))
        return false;

    f.iter().setResult(def);
    return true;
}

static bool
EmitBinaryMathBuiltinCall(FunctionCompiler& f, uint32_t callOffset, SymbolicAddress callee,
                          ValType operandType)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

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

    if (!f.finishCall(&call, PassTls::False))
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset, 0,
                             MembarBeforeLoad, MembarAfterLoad);
    f.iter().setResult(f.loadAtomicHeap(addr.base, access));
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset, 0,
                             MembarBeforeStore, MembarAfterStore);
    f.storeAtomicHeap(addr.base, access, value);
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset);
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
      case ValType::Limit:;
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

    MWasmMemoryAccess access(viewType, addr.align, addr.offset, numElems);
    f.iter().setResult(f.loadSimdHeap(addr.base, access));
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
    if (!f.iter().readStore(resultType, Scalar::byteSize(viewType), &addr, &value))
        return false;

    MWasmMemoryAccess access(viewType, addr.align, addr.offset, numElems);
    f.storeSimdHeap(addr.base, access, value);
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
      case ValType::Limit:
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
      case SimdOperation::Fn_load3:
        return EmitSimdLoad(f, type, 3);
      case SimdOperation::Fn_store:
        return EmitSimdStore(f, type, 0);
      case SimdOperation::Fn_store1:
        return EmitSimdStore(f, type, 1);
      case SimdOperation::Fn_store2:
        return EmitSimdStore(f, type, 2);
      case SimdOperation::Fn_store3:
        return EmitSimdStore(f, type, 3);
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
      case SimdOperation::Fn_fromFloat64x2Bits:
        MOZ_CRASH("NYI");
    }
    MOZ_CRASH("unexpected opcode");
}

static bool
EmitExpr(FunctionCompiler& f)
{
    if (!f.mirGen().ensureBallast())
        return false;

    uint32_t exprOffset = f.iter().currentOffset();

    Expr expr;
    if (!f.iter().readExpr(&expr))
        return false;

    switch (expr) {
      // Control opcodes
      case Expr::Nop:
        return f.iter().readNullary();
      case Expr::Block:
        return EmitBlock(f);
      case Expr::Loop:
        return EmitLoop(f);
      case Expr::If:
        return EmitIf(f);
      case Expr::Else:
        return EmitElse(f);
      case Expr::End:
        return EmitEnd(f);
      case Expr::Br:
        return EmitBr(f);
      case Expr::BrIf:
        return EmitBrIf(f);
      case Expr::BrTable:
        return EmitBrTable(f);
      case Expr::Return:
        return EmitReturn(f);
      case Expr::Unreachable:
        if (!f.iter().readUnreachable())
            return false;
        f.unreachableTrap();
        return true;

      // Calls
      case Expr::Call:
        return EmitCall(f, exprOffset);
      case Expr::CallIndirect:
        return EmitCallIndirect(f, exprOffset);
      case Expr::CallImport:
        return EmitCallImport(f, exprOffset);

      // Locals and globals
      case Expr::GetLocal:
        return EmitGetLocal(f);
      case Expr::SetLocal:
        return EmitSetLocal(f);
      case Expr::GetGlobal:
        return EmitGetGlobal(f);
      case Expr::SetGlobal:
        return EmitSetGlobal(f);

      // Select
      case Expr::Select:
        return EmitSelect(f);

      // I32
      case Expr::I32Const: {
        int32_t i32;
        if (!f.iter().readI32Const(&i32))
            return false;

        f.iter().setResult(f.constant(Int32Value(i32), MIRType::Int32));
        return true;
      }
      case Expr::I32Add:
        return EmitAdd(f, ValType::I32, MIRType::Int32);
      case Expr::I32Sub:
        return EmitSub(f, ValType::I32, MIRType::Int32);
      case Expr::I32Mul:
        return EmitMul(f, ValType::I32, MIRType::Int32);
      case Expr::I32DivS:
      case Expr::I32DivU:
        return EmitDiv(f, ValType::I32, MIRType::Int32, expr == Expr::I32DivU);
      case Expr::I32RemS:
      case Expr::I32RemU:
        return EmitRem(f, ValType::I32, MIRType::Int32, expr == Expr::I32RemU);
      case Expr::I32Min:
      case Expr::I32Max:
        return EmitMinMax(f, ValType::I32, MIRType::Int32, expr == Expr::I32Max);
      case Expr::I32Eqz:
        return EmitConversion<MNot>(f, ValType::I32, ValType::I32);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
        return EmitTruncate(f, ValType::F32, ValType::I32, expr == Expr::I32TruncUF32);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return EmitTruncate(f, ValType::F64, ValType::I32, expr == Expr::I32TruncUF64);
      case Expr::I32WrapI64:
        return EmitConversion<MWrapInt64ToInt32>(f, ValType::I64, ValType::I32);
      case Expr::I32ReinterpretF32:
        return EmitReinterpret(f, ValType::I32, ValType::F32, MIRType::Int32);
      case Expr::I32Clz:
        return EmitUnary<MClz>(f, ValType::I32);
      case Expr::I32Ctz:
        return EmitUnary<MCtz>(f, ValType::I32);
      case Expr::I32Popcnt:
        return EmitUnary<MPopcnt>(f, ValType::I32);
      case Expr::I32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::I32, MIRType::Int32);
      case Expr::I32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::I32, MIRType::Int32);
      case Expr::I32Or:
        return EmitBitwise<MBitOr>(f, ValType::I32, MIRType::Int32);
      case Expr::I32And:
        return EmitBitwise<MBitAnd>(f, ValType::I32, MIRType::Int32);
      case Expr::I32Xor:
        return EmitBitwise<MBitXor>(f, ValType::I32, MIRType::Int32);
      case Expr::I32Shl:
        return EmitBitwise<MLsh>(f, ValType::I32, MIRType::Int32);
      case Expr::I32ShrS:
        return EmitBitwise<MRsh>(f, ValType::I32, MIRType::Int32);
      case Expr::I32ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I32, MIRType::Int32);
      case Expr::I32BitNot:
        return EmitBitwise<MBitNot>(f, ValType::I32);
      case Expr::I32Load8S:
        return EmitLoad(f, ValType::I32, Scalar::Int8);
      case Expr::I32Load8U:
        return EmitLoad(f, ValType::I32, Scalar::Uint8);
      case Expr::I32Load16S:
        return EmitLoad(f, ValType::I32, Scalar::Int16);
      case Expr::I32Load16U:
        return EmitLoad(f, ValType::I32, Scalar::Uint16);
      case Expr::I32Load:
        return EmitLoad(f, ValType::I32, Scalar::Int32);
      case Expr::I32Store8:
        return EmitStore(f, ValType::I32, Scalar::Int8);
      case Expr::I32Store16:
        return EmitStore(f, ValType::I32, Scalar::Int16);
      case Expr::I32Store:
        return EmitStore(f, ValType::I32, Scalar::Int32);
      case Expr::I32Rotr:
      case Expr::I32Rotl:
        return EmitRotate(f, ValType::I32, expr == Expr::I32Rotl);

      // I64
      case Expr::I64Const: {
        int64_t i64;
        if (!f.iter().readI64Const(&i64))
            return false;

        f.iter().setResult(f.constant(i64));
        return true;
      }
      case Expr::I64Add:
        return EmitAdd(f, ValType::I64, MIRType::Int64);
      case Expr::I64Sub:
        return EmitSub(f, ValType::I64, MIRType::Int64);
      case Expr::I64Mul:
        return EmitMul(f, ValType::I64, MIRType::Int64);
      case Expr::I64DivS:
      case Expr::I64DivU:
        return EmitDiv(f, ValType::I64, MIRType::Int64, expr == Expr::I64DivU);
      case Expr::I64RemS:
      case Expr::I64RemU:
        return EmitRem(f, ValType::I64, MIRType::Int64, expr == Expr::I64RemU);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return EmitTruncate(f, ValType::F32, ValType::I64, expr == Expr::I64TruncUF32);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
        return EmitTruncate(f, ValType::F64, ValType::I64, expr == Expr::I64TruncUF64);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return EmitExtendI32(f, expr == Expr::I64ExtendUI32);
      case Expr::I64ReinterpretF64:
        return EmitReinterpret(f, ValType::I64, ValType::F64, MIRType::Int64);
      case Expr::I64Or:
        return EmitBitwise<MBitOr>(f, ValType::I64, MIRType::Int64);
      case Expr::I64And:
        return EmitBitwise<MBitAnd>(f, ValType::I64, MIRType::Int64);
      case Expr::I64Xor:
        return EmitBitwise<MBitXor>(f, ValType::I64, MIRType::Int64);
      case Expr::I64Shl:
        return EmitBitwise<MLsh>(f, ValType::I64, MIRType::Int64);
      case Expr::I64ShrS:
        return EmitBitwise<MRsh>(f, ValType::I64, MIRType::Int64);
      case Expr::I64ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I64, MIRType::Int64);
      case Expr::I64Rotr:
      case Expr::I64Rotl:
        return EmitRotate(f, ValType::I64, expr == Expr::I64Rotl);
      case Expr::I64Eqz:
        return EmitConversion<MNot>(f, ValType::I64, ValType::I32);
      case Expr::I64Clz:
        return EmitUnary<MClz>(f, ValType::I64);
      case Expr::I64Ctz:
        return EmitUnary<MCtz>(f, ValType::I64);
      case Expr::I64Popcnt:
        return EmitUnary<MPopcnt>(f, ValType::I64);
      case Expr::I64Load8S:
        return EmitLoad(f, ValType::I64, Scalar::Int8);
      case Expr::I64Load8U:
        return EmitLoad(f, ValType::I64, Scalar::Uint8);
      case Expr::I64Load16S:
        return EmitLoad(f, ValType::I64, Scalar::Int16);
      case Expr::I64Load16U:
        return EmitLoad(f, ValType::I64, Scalar::Uint16);
      case Expr::I64Load32S:
        return EmitLoad(f, ValType::I64, Scalar::Int32);
      case Expr::I64Load32U:
        return EmitLoad(f, ValType::I64, Scalar::Uint32);
      case Expr::I64Load:
        return EmitLoad(f, ValType::I64, Scalar::Int64);
      case Expr::I64Store8:
        return EmitStore(f, ValType::I64, Scalar::Int8);
      case Expr::I64Store16:
        return EmitStore(f, ValType::I64, Scalar::Int16);
      case Expr::I64Store32:
        return EmitStore(f, ValType::I64, Scalar::Int32);
      case Expr::I64Store:
        return EmitStore(f, ValType::I64, Scalar::Int64);

      // F32
      case Expr::F32Const: {
        float f32;
        if (!f.iter().readF32Const(&f32))
            return false;

        f.iter().setResult(f.constant(f32));
        return true;
      }
      case Expr::F32Add:
        return EmitAdd(f, ValType::F32, MIRType::Float32);
      case Expr::F32Sub:
        return EmitSub(f, ValType::F32, MIRType::Float32);
      case Expr::F32Mul:
        return EmitMul(f, ValType::F32, MIRType::Float32);
      case Expr::F32Div:
        return EmitDiv(f, ValType::F32, MIRType::Float32, /* isUnsigned = */ false);
      case Expr::F32Min:
      case Expr::F32Max:
        return EmitMinMax(f, ValType::F32, MIRType::Float32, expr == Expr::F32Max);
      case Expr::F32CopySign:
        return EmitCopySign(f, ValType::F32);
      case Expr::F32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F32, MIRType::Float32);
      case Expr::F32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F32, MIRType::Float32);
      case Expr::F32Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F32, MIRType::Float32);
      case Expr::F32Ceil:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::CeilF, ValType::F32);
      case Expr::F32Floor:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::FloorF, ValType::F32);
      case Expr::F32Trunc:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::TruncF, ValType::F32);
      case Expr::F32Nearest:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::NearbyIntF, ValType::F32);
      case Expr::F32DemoteF64:
        return EmitConversion<MToFloat32>(f, ValType::F64, ValType::F32);
      case Expr::F32ConvertSI32:
        return EmitConversion<MToFloat32>(f, ValType::I32, ValType::F32);
      case Expr::F32ConvertUI32:
        return EmitConversion<MAsmJSUnsignedToFloat32>(f, ValType::I32, ValType::F32);
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F32, MIRType::Float32,
                                             expr == Expr::F32ConvertUI64);
      case Expr::F32ReinterpretI32:
        return EmitReinterpret(f, ValType::F32, ValType::I32, MIRType::Float32);

      case Expr::F32Load:
        return EmitLoad(f, ValType::F32, Scalar::Float32);
      case Expr::F32Store:
        return EmitStore(f, ValType::F32, Scalar::Float32);
      case Expr::F32StoreF64:
        return EmitStoreWithCoercion(f, ValType::F32, Scalar::Float64);

      // F64
      case Expr::F64Const: {
        double f64;
        if (!f.iter().readF64Const(&f64))
            return false;

        f.iter().setResult(f.constant(f64));
        return true;
      }
      case Expr::F64Add:
        return EmitAdd(f, ValType::F64, MIRType::Double);
      case Expr::F64Sub:
        return EmitSub(f, ValType::F64, MIRType::Double);
      case Expr::F64Mul:
        return EmitMul(f, ValType::F64, MIRType::Double);
      case Expr::F64Div:
        return EmitDiv(f, ValType::F64, MIRType::Double, /* isUnsigned = */ false);
      case Expr::F64Mod:
        return EmitRem(f, ValType::F64, MIRType::Double, /* isUnsigned = */ false);
      case Expr::F64Min:
      case Expr::F64Max:
        return EmitMinMax(f, ValType::F64, MIRType::Double, expr == Expr::F64Max);
      case Expr::F64CopySign:
        return EmitCopySign(f, ValType::F64);
      case Expr::F64Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F64, MIRType::Double);
      case Expr::F64Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F64, MIRType::Double);
      case Expr::F64Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F64, MIRType::Double);
      case Expr::F64Ceil:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::CeilD, ValType::F64);
      case Expr::F64Floor:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::FloorD, ValType::F64);
      case Expr::F64Trunc:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::TruncD, ValType::F64);
      case Expr::F64Nearest:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::NearbyIntD, ValType::F64);
      case Expr::F64Sin:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::SinD, ValType::F64);
      case Expr::F64Cos:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::CosD, ValType::F64);
      case Expr::F64Tan:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::TanD, ValType::F64);
      case Expr::F64Asin:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::ASinD, ValType::F64);
      case Expr::F64Acos:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::ACosD, ValType::F64);
      case Expr::F64Atan:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::ATanD, ValType::F64);
      case Expr::F64Exp:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::ExpD, ValType::F64);
      case Expr::F64Log:
        return EmitUnaryMathBuiltinCall(f, exprOffset, SymbolicAddress::LogD, ValType::F64);
      case Expr::F64Pow:
        return EmitBinaryMathBuiltinCall(f, exprOffset, SymbolicAddress::PowD, ValType::F64);
      case Expr::F64Atan2:
        return EmitBinaryMathBuiltinCall(f, exprOffset, SymbolicAddress::ATan2D,
                                         ValType::F64);
      case Expr::F64PromoteF32:
        return EmitConversion<MToDouble>(f, ValType::F32, ValType::F64);
      case Expr::F64ConvertSI32:
        return EmitConversion<MToDouble>(f, ValType::I32, ValType::F64);
      case Expr::F64ConvertUI32:
        return EmitConversion<MAsmJSUnsignedToDouble>(f, ValType::I32, ValType::F64);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F64, MIRType::Double,
                                             expr == Expr::F64ConvertUI64);
      case Expr::F64Load:
        return EmitLoad(f, ValType::F64, Scalar::Float64);
      case Expr::F64Store:
        return EmitStore(f, ValType::F64, Scalar::Float64);
      case Expr::F64StoreF32:
        return EmitStoreWithCoercion(f, ValType::F64, Scalar::Float32);
      case Expr::F64ReinterpretI64:
        return EmitReinterpret(f, ValType::F64, ValType::I64, MIRType::Double);

      // Comparisons
      case Expr::I32Eq:
        return EmitComparison(f, ValType::I32, JSOP_EQ, MCompare::Compare_Int32);
      case Expr::I32Ne:
        return EmitComparison(f, ValType::I32, JSOP_NE, MCompare::Compare_Int32);
      case Expr::I32LtS:
        return EmitComparison(f, ValType::I32, JSOP_LT, MCompare::Compare_Int32);
      case Expr::I32LeS:
        return EmitComparison(f, ValType::I32, JSOP_LE, MCompare::Compare_Int32);
      case Expr::I32GtS:
        return EmitComparison(f, ValType::I32, JSOP_GT, MCompare::Compare_Int32);
      case Expr::I32GeS:
        return EmitComparison(f, ValType::I32, JSOP_GE, MCompare::Compare_Int32);
      case Expr::I32LtU:
        return EmitComparison(f, ValType::I32, JSOP_LT, MCompare::Compare_UInt32);
      case Expr::I32LeU:
        return EmitComparison(f, ValType::I32, JSOP_LE, MCompare::Compare_UInt32);
      case Expr::I32GtU:
        return EmitComparison(f, ValType::I32, JSOP_GT, MCompare::Compare_UInt32);
      case Expr::I32GeU:
        return EmitComparison(f, ValType::I32, JSOP_GE, MCompare::Compare_UInt32);
      case Expr::I64Eq:
        return EmitComparison(f, ValType::I64, JSOP_EQ, MCompare::Compare_Int64);
      case Expr::I64Ne:
        return EmitComparison(f, ValType::I64, JSOP_NE, MCompare::Compare_Int64);
      case Expr::I64LtS:
        return EmitComparison(f, ValType::I64, JSOP_LT, MCompare::Compare_Int64);
      case Expr::I64LeS:
        return EmitComparison(f, ValType::I64, JSOP_LE, MCompare::Compare_Int64);
      case Expr::I64GtS:
        return EmitComparison(f, ValType::I64, JSOP_GT, MCompare::Compare_Int64);
      case Expr::I64GeS:
        return EmitComparison(f, ValType::I64, JSOP_GE, MCompare::Compare_Int64);
      case Expr::I64LtU:
        return EmitComparison(f, ValType::I64, JSOP_LT, MCompare::Compare_UInt64);
      case Expr::I64LeU:
        return EmitComparison(f, ValType::I64, JSOP_LE, MCompare::Compare_UInt64);
      case Expr::I64GtU:
        return EmitComparison(f, ValType::I64, JSOP_GT, MCompare::Compare_UInt64);
      case Expr::I64GeU:
        return EmitComparison(f, ValType::I64, JSOP_GE, MCompare::Compare_UInt64);
      case Expr::F32Eq:
        return EmitComparison(f, ValType::F32, JSOP_EQ, MCompare::Compare_Float32);
      case Expr::F32Ne:
        return EmitComparison(f, ValType::F32, JSOP_NE, MCompare::Compare_Float32);
      case Expr::F32Lt:
        return EmitComparison(f, ValType::F32, JSOP_LT, MCompare::Compare_Float32);
      case Expr::F32Le:
        return EmitComparison(f, ValType::F32, JSOP_LE, MCompare::Compare_Float32);
      case Expr::F32Gt:
        return EmitComparison(f, ValType::F32, JSOP_GT, MCompare::Compare_Float32);
      case Expr::F32Ge:
        return EmitComparison(f, ValType::F32, JSOP_GE, MCompare::Compare_Float32);
      case Expr::F64Eq:
        return EmitComparison(f, ValType::F64, JSOP_EQ, MCompare::Compare_Double);
      case Expr::F64Ne:
        return EmitComparison(f, ValType::F64, JSOP_NE, MCompare::Compare_Double);
      case Expr::F64Lt:
        return EmitComparison(f, ValType::F64, JSOP_LT, MCompare::Compare_Double);
      case Expr::F64Le:
        return EmitComparison(f, ValType::F64, JSOP_LE, MCompare::Compare_Double);
      case Expr::F64Gt:
        return EmitComparison(f, ValType::F64, JSOP_GT, MCompare::Compare_Double);
      case Expr::F64Ge:
        return EmitComparison(f, ValType::F64, JSOP_GE, MCompare::Compare_Double);

      // SIMD
#define CASE(TYPE, OP, SIGN)                                                    \
      case Expr::TYPE##OP:                                                      \
        return EmitSimdOp(f, ValType::TYPE, SimdOperation::Fn_##OP, SIGN);
#define I8x16CASE(OP) CASE(I8x16, OP, SimdSign::Signed)
#define I16x8CASE(OP) CASE(I16x8, OP, SimdSign::Signed)
#define I32x4CASE(OP) CASE(I32x4, OP, SimdSign::Signed)
#define F32x4CASE(OP) CASE(F32x4, OP, SimdSign::NotApplicable)
#define B8x16CASE(OP) CASE(B8x16, OP, SimdSign::NotApplicable)
#define B16x8CASE(OP) CASE(B16x8, OP, SimdSign::NotApplicable)
#define B32x4CASE(OP) CASE(B32x4, OP, SimdSign::NotApplicable)
#define ENUMERATE(TYPE, FORALL, DO)                                             \
      case Expr::TYPE##Constructor:                                             \
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

      case Expr::I8x16Const: {
        I8x16 i8x16;
        if (!f.iter().readI8x16Const(&i8x16))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX16(i8x16), MIRType::Int8x16));
        return true;
      }
      case Expr::I16x8Const: {
        I16x8 i16x8;
        if (!f.iter().readI16x8Const(&i16x8))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX8(i16x8), MIRType::Int16x8));
        return true;
      }
      case Expr::I32x4Const: {
        I32x4 i32x4;
        if (!f.iter().readI32x4Const(&i32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(i32x4), MIRType::Int32x4));
        return true;
      }
      case Expr::F32x4Const: {
        F32x4 f32x4;
        if (!f.iter().readF32x4Const(&f32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(f32x4), MIRType::Float32x4));
        return true;
      }
      case Expr::B8x16Const: {
        I8x16 i8x16;
        if (!f.iter().readB8x16Const(&i8x16))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX16(i8x16), MIRType::Bool8x16));
        return true;
      }
      case Expr::B16x8Const: {
        I16x8 i16x8;
        if (!f.iter().readB16x8Const(&i16x8))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX8(i16x8), MIRType::Bool16x8));
        return true;
      }
      case Expr::B32x4Const: {
        I32x4 i32x4;
        if (!f.iter().readB32x4Const(&i32x4))
            return false;

        f.iter().setResult(f.constant(SimdConstant::CreateX4(i32x4), MIRType::Bool32x4));
        return true;
      }

      // SIMD unsigned integer operations.
      case Expr::I8x16addSaturateU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_addSaturate, SimdSign::Unsigned);
      case Expr::I8x16subSaturateU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_subSaturate, SimdSign::Unsigned);
      case Expr::I8x16shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Expr::I8x16lessThanU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Expr::I8x16lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Expr::I8x16greaterThanU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Expr::I8x16greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Expr::I8x16extractLaneU:
        return EmitSimdOp(f, ValType::I8x16, SimdOperation::Fn_extractLane, SimdSign::Unsigned);

      case Expr::I16x8addSaturateU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_addSaturate, SimdSign::Unsigned);
      case Expr::I16x8subSaturateU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_subSaturate, SimdSign::Unsigned);
      case Expr::I16x8shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Expr::I16x8lessThanU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Expr::I16x8lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Expr::I16x8greaterThanU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Expr::I16x8greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Expr::I16x8extractLaneU:
        return EmitSimdOp(f, ValType::I16x8, SimdOperation::Fn_extractLane, SimdSign::Unsigned);

      case Expr::I32x4shiftRightByScalarU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_shiftRightByScalar, SimdSign::Unsigned);
      case Expr::I32x4lessThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThan, SimdSign::Unsigned);
      case Expr::I32x4lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThanOrEqual, SimdSign::Unsigned);
      case Expr::I32x4greaterThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThan, SimdSign::Unsigned);
      case Expr::I32x4greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThanOrEqual, SimdSign::Unsigned);
      case Expr::I32x4fromFloat32x4U:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_fromFloat32x4, SimdSign::Unsigned);

      // Atomics
      case Expr::I32AtomicsLoad:
        return EmitAtomicsLoad(f);
      case Expr::I32AtomicsStore:
        return EmitAtomicsStore(f);
      case Expr::I32AtomicsBinOp:
        return EmitAtomicsBinOp(f);
      case Expr::I32AtomicsCompareExchange:
        return EmitAtomicsCompareExchange(f);
      case Expr::I32AtomicsExchange:
        return EmitAtomicsExchange(f);

      // Future opcodes
      case Expr::CurrentMemory:
      case Expr::GrowMemory:
        MOZ_CRASH("NYI");
      case Expr::Limit:;
    }

    MOZ_CRASH("unexpected wasm opcode");
}

bool
wasm::IonCompileFunction(IonCompileTask* task)
{
    MOZ_ASSERT(task->mode() == IonCompileTask::CompileMode::Ion);

    const FuncBytes& func = task->func();
    FuncCompileResults& results = task->results();

    Decoder d(func.bytes());

    // Build the local types vector.

    ValTypeVector locals;
    if (!locals.appendAll(func.sig().args()))
        return false;
    if (!DecodeLocalEntries(d, &locals))
        return false;

    // Set up for Ion compilation.

    JitContext jitContext(&results.alloc());
    const JitCompileOptions options;
    MIRGraph graph(&results.alloc());
    CompileInfo compileInfo(locals.length());
    MIRGenerator mir(nullptr, options, &results.alloc(), &graph, &compileInfo,
                     IonOptimizations.get(OptimizationLevel::AsmJS));
    mir.initUsesSignalHandlersForAsmJSOOB(task->mg().usesSignal.forOOB);
    mir.initMinAsmJSHeapLength(task->mg().minMemoryLength);

    // Build MIR graph
    {
        FunctionCompiler f(task->mg(), d, func, locals, mir, results);
        if (!f.init())
            return false;

        if (!f.iter().readFunctionStart())
            return false;

        while (!f.done()) {
            if (!EmitExpr(f))
                return false;
        }

        MDefinition* value;
        if (!f.iter().readFunctionEnd(f.sig().ret(), &value))
            return false;
        if (IsVoid(f.sig().ret()))
            f.returnVoid();
        else
            f.returnExpr(value);

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

        SigIdDesc sigId = task->mg().funcSigs[func.index()]->id;

        CodeGenerator codegen(&mir, lir, &results.masm());
        if (!codegen.generateWasm(sigId, &results.offsets()))
            return false;
    }

    return true;
}

bool
wasm::CompileFunction(IonCompileTask* task)
{
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logCompile(logger, TraceLogger_WasmCompilation);

    switch (task->mode()) {
      case wasm::IonCompileTask::CompileMode::Ion:
        return wasm::IonCompileFunction(task);
      case wasm::IonCompileTask::CompileMode::Baseline:
        return wasm::BaselineCompileFunction(task);
      case wasm::IonCompileTask::CompileMode::None:
        break;
    }

    MOZ_CRASH("Uninitialized task");
}
