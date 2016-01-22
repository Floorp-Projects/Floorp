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

#include "jit/CodeGenerator.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::DebugOnly;
using mozilla::Maybe;

typedef Vector<size_t, 1, SystemAllocPolicy> LabelVector;
typedef Vector<MBasicBlock*, 8, SystemAllocPolicy> BlockVector;

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph.
class FunctionCompiler
{
  private:
    typedef HashMap<uint32_t, BlockVector, DefaultHasher<uint32_t>, SystemAllocPolicy> LabeledBlockMap;
    typedef HashMap<size_t, BlockVector, DefaultHasher<uint32_t>, SystemAllocPolicy> UnlabeledBlockMap;
    typedef Vector<size_t, 4, SystemAllocPolicy> PositionStack;

    ModuleGeneratorThreadView& mg_;
    const FuncBytecode&        func_;
    Decoder                    decoder_;
    size_t                     nextId_;
    size_t                     lastReadCallSite_;

    TempAllocator&             alloc_;
    MIRGraph&                  graph_;
    const CompileInfo&         info_;
    MIRGenerator&              mirGen_;

    MBasicBlock*               curBlock_;

    PositionStack              loopStack_;
    PositionStack              breakableStack_;
    UnlabeledBlockMap          unlabeledBreaks_;
    UnlabeledBlockMap          unlabeledContinues_;
    LabeledBlockMap            labeledBreaks_;
    LabeledBlockMap            labeledContinues_;

    FuncCompileResults&        compileResults_;

  public:
    FunctionCompiler(ModuleGeneratorThreadView& mg, const FuncBytecode& func, MIRGenerator& mirGen,
                     FuncCompileResults& compileResults)
      : mg_(mg),
        func_(func),
        decoder_(func.bytecode()),
        nextId_(0),
        lastReadCallSite_(0),
        alloc_(mirGen.alloc()),
        graph_(mirGen.graph()),
        info_(mirGen.info()),
        mirGen_(mirGen),
        curBlock_(nullptr),
        compileResults_(compileResults)
    {}

    ModuleGeneratorThreadView& mg() const    { return mg_; }
    TempAllocator&             alloc() const { return alloc_; }
    MacroAssembler&            masm() const  { return compileResults_.masm(); }
    const Sig&                 sig() const   { return func_.sig(); }

    bool init()
    {
        if (!unlabeledBreaks_.init() ||
            !unlabeledContinues_.init() ||
            !labeledBreaks_.init() ||
            !labeledContinues_.init())
        {
            return false;
        }

        // Prepare the entry block for MIR generation:

        const ValTypeVector& args = func_.sig().args();

        if (!mirGen_.ensureBallast())
            return false;
        if (!newBlock(/* pred = */ nullptr, &curBlock_))
            return false;

        for (ABIArgValTypeIter i(args); !i.done(); i++) {
            MAsmJSParameter* ins = MAsmJSParameter::New(alloc(), *i, i.mirType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i.index()), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        for (size_t i = args.length(); i < func_.numLocals(); i++) {
            MInstruction* ins = nullptr;
            switch (func_.localType(i)) {
              case ValType::I32:
                ins = MConstant::NewAsmJS(alloc(), Int32Value(0), MIRType_Int32);
                break;
              case ValType::I64:
                MOZ_CRASH("int64");
              case ValType::F32:
                ins = MConstant::NewAsmJS(alloc(), Float32Value(0.f), MIRType_Float32);
                break;
              case ValType::F64:
                ins = MConstant::NewAsmJS(alloc(), DoubleValue(0.0), MIRType_Double);
                break;
              case ValType::I32x4:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0), MIRType_Int32x4);
                break;
              case ValType::F32x4:
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0.f), MIRType_Float32x4);
                break;
              case ValType::B32x4:
                // Bool32x4 uses the same data layout as Int32x4.
                ins = MSimdConstant::New(alloc(), SimdConstant::SplatX4(0), MIRType_Bool32x4);
                break;
            }

            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        return true;
    }

    void checkPostconditions()
    {
        MOZ_ASSERT(loopStack_.empty());
        MOZ_ASSERT(unlabeledBreaks_.empty());
        MOZ_ASSERT(unlabeledContinues_.empty());
        MOZ_ASSERT(labeledBreaks_.empty());
        MOZ_ASSERT(labeledContinues_.empty());
        MOZ_ASSERT(inDeadCode());
        MOZ_ASSERT(decoder_.done(), "all bytecode must be consumed");
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

    ExprType localType(unsigned slot) const { return ToExprType(func_.localType(slot)); }

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

    MDefinition* unarySimd(MDefinition* input, MSimdUnaryArith::Operation op, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(input->type()) && input->type() == type);
        MInstruction* ins = MSimdUnaryArith::NewAsmJS(alloc(), input, op, type);
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
        MSimdBinaryArith* ins = MSimdBinaryArith::NewAsmJS(alloc(), lhs, rhs, op, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* binarySimd(MDefinition* lhs, MDefinition* rhs, MSimdBinaryBitwise::Operation op,
                            MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(lhs->type()) && rhs->type() == lhs->type());
        MOZ_ASSERT(lhs->type() == type);
        MSimdBinaryBitwise* ins = MSimdBinaryBitwise::NewAsmJS(alloc(), lhs, rhs, op, type);
        curBlock_->add(ins);
        return ins;
    }

    template<class T>
    MDefinition* binarySimd(MDefinition* lhs, MDefinition* rhs, typename T::Operation op)
    {
        if (inDeadCode())
            return nullptr;

        T* ins = T::NewAsmJS(alloc(), lhs, rhs, op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* swizzleSimd(MDefinition* vector, int32_t X, int32_t Y, int32_t Z, int32_t W,
                             MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MSimdSwizzle* ins = MSimdSwizzle::New(alloc(), vector, type, X, Y, Z, W);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* shuffleSimd(MDefinition* lhs, MDefinition* rhs, int32_t X, int32_t Y,
                             int32_t Z, int32_t W, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MInstruction* ins = MSimdShuffle::New(alloc(), lhs, rhs, type, X, Y, Z, W);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* insertElementSimd(MDefinition* vec, MDefinition* val, SimdLane lane, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(vec->type()) && vec->type() == type);
        MOZ_ASSERT(!IsSimdType(val->type()));
        MSimdInsertElement* ins = MSimdInsertElement::NewAsmJS(alloc(), vec, val, type, lane);
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
        MSimdSelect* ins = MSimdSelect::NewAsmJS(alloc(), mask, lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* simdAllTrue(MDefinition* boolVector)
    {
        if (inDeadCode())
            return nullptr;

        MSimdAllTrue* ins = MSimdAllTrue::NewAsmJS(alloc(), boolVector);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* simdAnyTrue(MDefinition* boolVector)
    {
        if (inDeadCode())
            return nullptr;

        MSimdAnyTrue* ins = MSimdAnyTrue::NewAsmJS(alloc(), boolVector);
        curBlock_->add(ins);
        return ins;
    }

    template<class T>
    MDefinition* convertSimd(MDefinition* vec, MIRType from, MIRType to)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(from) && IsSimdType(to) && from != to);
        T* ins = T::NewAsmJS(alloc(), vec, from, to);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* splatSimd(MDefinition* v, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(type));
        MSimdSplatX4* ins = MSimdSplatX4::NewAsmJS(alloc(), v, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* minMax(MDefinition* lhs, MDefinition* rhs, MIRType type, bool isMax) {
        if (inDeadCode())
            return nullptr;
        MMinMax* ins = MMinMax::New(alloc(), lhs, rhs, type, isMax);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* mul(MDefinition* lhs, MDefinition* rhs, MIRType type, MMul::Mode mode)
    {
        if (inDeadCode())
            return nullptr;
        MMul* ins = MMul::New(alloc(), lhs, rhs, type, mode);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* div(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        MDiv* ins = MDiv::NewAsmJS(alloc(), lhs, rhs, type, unsignd);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* mod(MDefinition* lhs, MDefinition* rhs, MIRType type, bool unsignd)
    {
        if (inDeadCode())
            return nullptr;
        MMod* ins = MMod::NewAsmJS(alloc(), lhs, rhs, type, unsignd);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition* bitwise(MDefinition* lhs, MDefinition* rhs)
    {
        if (inDeadCode())
            return nullptr;
        T* ins = T::NewAsmJS(alloc(), lhs, rhs);
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

    MDefinition* loadHeap(Scalar::Type accessType, MDefinition* ptr, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD loads should use loadSimdHeap");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr, needsBoundsCheck);
        curBlock_->add(load);
        return load;
    }

    MDefinition* loadSimdHeap(Scalar::Type accessType, MDefinition* ptr, NeedsBoundsCheck chk,
                              unsigned numElems)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MOZ_ASSERT(Scalar::isSimdType(accessType), "loadSimdHeap can only load from a SIMD view");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr, needsBoundsCheck,
                                                   numElems);
        curBlock_->add(load);
        return load;
    }

    void storeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD stores should use loadSimdHeap");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v, needsBoundsCheck);
        curBlock_->add(store);
    }

    void storeSimdHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v,
                       NeedsBoundsCheck chk, unsigned numElems)
    {
        if (inDeadCode())
            return;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MOZ_ASSERT(Scalar::isSimdType(accessType), "storeSimdHeap can only load from a SIMD view");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v, needsBoundsCheck,
                                                      numElems);
        curBlock_->add(store);
    }

    void memoryBarrier(MemoryBarrierBits type)
    {
        if (inDeadCode())
            return;
        MMemoryBarrier* ins = MMemoryBarrier::New(alloc(), type);
        curBlock_->add(ins);
    }

    MDefinition* atomicLoadHeap(Scalar::Type accessType, MDefinition* ptr, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr, needsBoundsCheck,
                                                   /* numElems */ 0,
                                                   MembarBeforeLoad, MembarAfterLoad);
        curBlock_->add(load);
        return load;
    }

    void atomicStoreHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v, needsBoundsCheck,
                                                      /* numElems = */ 0,
                                                      MembarBeforeStore, MembarAfterStore);
        curBlock_->add(store);
    }

    MDefinition* atomicCompareExchangeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* oldv,
                                           MDefinition* newv, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MAsmJSCompareExchangeHeap* cas =
            MAsmJSCompareExchangeHeap::New(alloc(), accessType, ptr, oldv, newv, needsBoundsCheck);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicExchangeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* value,
                                    NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MAsmJSAtomicExchangeHeap* cas =
            MAsmJSAtomicExchangeHeap::New(alloc(), accessType, ptr, value, needsBoundsCheck);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicBinopHeap(js::jit::AtomicOp op, Scalar::Type accessType, MDefinition* ptr,
                                 MDefinition* v, NeedsBoundsCheck chk)
    {
        if (inDeadCode())
            return nullptr;

        bool needsBoundsCheck = chk == NEEDS_BOUNDS_CHECK;
        MAsmJSAtomicBinopHeap* binop =
            MAsmJSAtomicBinopHeap::New(alloc(), op, accessType, ptr, v, needsBoundsCheck);
        curBlock_->add(binop);
        return binop;
    }

    MDefinition* loadGlobalVar(unsigned globalDataOffset, bool isConst, MIRType type)
    {
        if (inDeadCode())
            return nullptr;
        MAsmJSLoadGlobalVar* load = MAsmJSLoadGlobalVar::New(alloc(), type, globalDataOffset,
                                                             isConst);
        curBlock_->add(load);
        return load;
    }

    void storeGlobalVar(uint32_t globalDataOffset, MDefinition* v)
    {
        if (inDeadCode())
            return;
        curBlock_->add(MAsmJSStoreGlobalVar::New(alloc(), globalDataOffset, v));
    }

    void addInterruptCheck(unsigned lineno, unsigned column)
    {
        if (inDeadCode())
            return;

        CallSiteDesc callDesc(lineno, column, CallSiteDesc::Relative);
        curBlock_->add(MAsmJSInterruptCheck::New(alloc(), masm().asmSyncInterruptLabel(), callDesc));
    }

    MDefinition* extractSimdElement(SimdLane lane, MDefinition* base, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(IsSimdType(base->type()));
        MOZ_ASSERT(!IsSimdType(type));
        MSimdExtractElement* ins = MSimdExtractElement::NewAsmJS(alloc(), base, type, lane);
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
        T* ins = T::NewAsmJS(alloc(), type, x, y, z, w);
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

    class Call
    {
        uint32_t lineno_;
        uint32_t column_;
        ABIArgGenerator abi_;
        uint32_t prevMaxStackBytes_;
        uint32_t maxChildStackBytes_;
        uint32_t spIncrement_;
        MAsmJSCall::Args regArgs_;
        Vector<MAsmJSPassStackArg*, 0, SystemAllocPolicy> stackArgs_;
        bool childClobbers_;

        friend class FunctionCompiler;

      public:
        Call(FunctionCompiler& f, uint32_t lineno, uint32_t column)
          : lineno_(lineno),
            column_(column),
            prevMaxStackBytes_(0),
            maxChildStackBytes_(0),
            spIncrement_(0),
            childClobbers_(false)
        { }
    };

    void startCallArgs(Call* call)
    {
        if (inDeadCode())
            return;
        call->prevMaxStackBytes_ = mirGen().resetAsmJSMaxStackArgBytes();
    }

    bool passArg(MDefinition* argDef, ValType type, Call* call)
    {
        if (inDeadCode())
            return true;

        uint32_t childStackBytes = mirGen().resetAsmJSMaxStackArgBytes();
        call->maxChildStackBytes_ = Max(call->maxChildStackBytes_, childStackBytes);
        if (childStackBytes > 0 && !call->stackArgs_.empty())
            call->childClobbers_ = true;

        ABIArg arg = call->abi_.next(ToMIRType(type));
        if (arg.kind() == ABIArg::Stack) {
            MAsmJSPassStackArg* mir = MAsmJSPassStackArg::New(alloc(), arg.offsetFromArgBase(),
                                                              argDef);
            curBlock_->add(mir);
            if (!call->stackArgs_.append(mir))
                return false;
        } else {
            if (!call->regArgs_.append(MAsmJSCall::Arg(arg.reg(), argDef)))
                return false;
        }
        return true;
    }

    void finishCallArgs(Call* call)
    {
        if (inDeadCode())
            return;
        uint32_t parentStackBytes = call->abi_.stackBytesConsumedSoFar();
        uint32_t newStackBytes;
        if (call->childClobbers_) {
            call->spIncrement_ = AlignBytes(call->maxChildStackBytes_, AsmJSStackAlignment);
            for (unsigned i = 0; i < call->stackArgs_.length(); i++)
                call->stackArgs_[i]->incrementOffset(call->spIncrement_);
            newStackBytes = Max(call->prevMaxStackBytes_,
                                call->spIncrement_ + parentStackBytes);
        } else {
            call->spIncrement_ = 0;
            newStackBytes = Max(call->prevMaxStackBytes_,
                                Max(call->maxChildStackBytes_, parentStackBytes));
        }
        mirGen_.setAsmJSMaxStackArgBytes(newStackBytes);
    }

  private:
    bool callPrivate(MAsmJSCall::Callee callee, const Call& call, ExprType ret, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        CallSiteDesc::Kind kind = CallSiteDesc::Kind(-1);
        switch (callee.which()) {
          case MAsmJSCall::Callee::Internal: kind = CallSiteDesc::Relative; break;
          case MAsmJSCall::Callee::Dynamic:  kind = CallSiteDesc::Register; break;
          case MAsmJSCall::Callee::Builtin:  kind = CallSiteDesc::Register; break;
        }

        MAsmJSCall* ins = MAsmJSCall::New(alloc(), CallSiteDesc(call.lineno_, call.column_, kind),
                                          callee, call.regArgs_, ToMIRType(ret), call.spIncrement_);
        if (!ins)
            return false;

        curBlock_->add(ins);
        *def = ins;
        return true;
    }

  public:
    bool internalCall(const Sig& sig, uint32_t funcIndex, const Call& call, MDefinition** def)
    {
        return callPrivate(MAsmJSCall::Callee(AsmJSInternalCallee(funcIndex)), call, sig.ret(), def);
    }

    bool funcPtrCall(const Sig& sig, uint32_t maskLit, uint32_t globalDataOffset, MDefinition* index,
                     const Call& call, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MConstant* mask = MConstant::New(alloc(), Int32Value(maskLit));
        curBlock_->add(mask);
        MBitAnd* maskedIndex = MBitAnd::NewAsmJS(alloc(), index, mask);
        curBlock_->add(maskedIndex);
        MAsmJSLoadFuncPtr* ptrFun = MAsmJSLoadFuncPtr::New(alloc(), globalDataOffset, maskedIndex);
        curBlock_->add(ptrFun);

        return callPrivate(MAsmJSCall::Callee(ptrFun), call, sig.ret(), def);
    }

    bool ffiCall(unsigned globalDataOffset, const Call& call, ExprType ret, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MAsmJSLoadFFIFunc* ptrFun = MAsmJSLoadFFIFunc::New(alloc(), globalDataOffset);
        curBlock_->add(ptrFun);

        return callPrivate(MAsmJSCall::Callee(ptrFun), call, ret, def);
    }

    bool builtinCall(SymbolicAddress builtin, const Call& call, ValType type, MDefinition** def)
    {
        return callPrivate(MAsmJSCall::Callee(builtin), call, ToExprType(type), def);
    }

    /*********************************************** Control flow generation */

    inline bool inDeadCode() const {
        return curBlock_ == nullptr;
    }

    void returnExpr(MDefinition* expr)
    {
        if (inDeadCode())
            return;
        MAsmJSReturn* ins = MAsmJSReturn::New(alloc(), expr);
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    void returnVoid()
    {
        if (inDeadCode())
            return;
        MAsmJSVoidReturn* ins = MAsmJSVoidReturn::New(alloc());
        curBlock_->end(ins);
        curBlock_ = nullptr;
    }

    bool branchAndStartThen(MDefinition* cond, MBasicBlock** thenBlock, MBasicBlock** elseBlock)
    {
        if (inDeadCode())
            return true;

        bool hasThenBlock = *thenBlock != nullptr;
        bool hasElseBlock = *elseBlock != nullptr;

        if (!hasThenBlock && !newBlock(curBlock_, thenBlock))
            return false;
        if (!hasElseBlock && !newBlock(curBlock_, elseBlock))
            return false;

        curBlock_->end(MTest::New(alloc(), cond, *thenBlock, *elseBlock));

        // Only add as a predecessor if newBlock hasn't been called (as it does it for us)
        if (hasThenBlock && !(*thenBlock)->addPredecessor(alloc(), curBlock_))
            return false;
        if (hasElseBlock && !(*elseBlock)->addPredecessor(alloc(), curBlock_))
            return false;

        curBlock_ = *thenBlock;
        mirGraph().moveBlockToEnd(curBlock_);
        return true;
    }

    void assertCurrentBlockIs(MBasicBlock* block) {
        if (inDeadCode())
            return;
        MOZ_ASSERT(curBlock_ == block);
    }

    bool appendThenBlock(BlockVector* thenBlocks)
    {
        if (inDeadCode())
            return true;
        return thenBlocks->append(curBlock_);
    }

    bool joinIf(const BlockVector& thenBlocks, MBasicBlock* joinBlock)
    {
        if (!joinBlock)
            return true;
        MOZ_ASSERT_IF(curBlock_, thenBlocks.back() == curBlock_);
        for (size_t i = 0; i < thenBlocks.length(); i++) {
            thenBlocks[i]->end(MGoto::New(alloc(), joinBlock));
            if (!joinBlock->addPredecessor(alloc(), thenBlocks[i]))
                return false;
        }
        curBlock_ = joinBlock;
        mirGraph().moveBlockToEnd(curBlock_);
        return true;
    }

    void switchToElse(MBasicBlock* elseBlock)
    {
        if (!elseBlock)
            return;
        curBlock_ = elseBlock;
        mirGraph().moveBlockToEnd(curBlock_);
    }

    bool joinIfElse(const BlockVector& thenBlocks)
    {
        if (inDeadCode() && thenBlocks.empty())
            return true;
        MBasicBlock* pred = curBlock_ ? curBlock_ : thenBlocks[0];
        MBasicBlock* join;
        if (!newBlock(pred, &join))
            return false;
        if (curBlock_)
            curBlock_->end(MGoto::New(alloc(), join));
        for (size_t i = 0; i < thenBlocks.length(); i++) {
            thenBlocks[i]->end(MGoto::New(alloc(), join));
            if (pred == curBlock_ || i > 0) {
                if (!join->addPredecessor(alloc(), thenBlocks[i]))
                    return false;
            }
        }
        curBlock_ = join;
        return true;
    }

    void pushPhiInput(MDefinition* def)
    {
        if (inDeadCode())
            return;
        MOZ_ASSERT(curBlock_->stackDepth() == info().firstStackSlot());
        curBlock_->push(def);
    }

    MDefinition* popPhiOutput()
    {
        if (inDeadCode())
            return nullptr;
        MOZ_ASSERT(curBlock_->stackDepth() == info().firstStackSlot() + 1);
        return curBlock_->pop();
    }

    bool startPendingLoop(size_t id, MBasicBlock** loopEntry)
    {
        if (!loopStack_.append(id) || !breakableStack_.append(id))
            return false;
        if (inDeadCode()) {
            *loopEntry = nullptr;
            return true;
        }
        MOZ_ASSERT(curBlock_->loopDepth() == loopStack_.length() - 1);
        *loopEntry = MBasicBlock::NewAsmJS(mirGraph(), info(), curBlock_,
                                           MBasicBlock::PENDING_LOOP_HEADER);
        if (!*loopEntry)
            return false;
        mirGraph().addBlock(*loopEntry);
        (*loopEntry)->setLoopDepth(loopStack_.length());
        curBlock_->end(MGoto::New(alloc(), *loopEntry));
        curBlock_ = *loopEntry;
        return true;
    }

    bool branchAndStartLoopBody(MDefinition* cond, MBasicBlock** afterLoop)
    {
        if (inDeadCode()) {
            *afterLoop = nullptr;
            return true;
        }
        MOZ_ASSERT(curBlock_->loopDepth() > 0);
        MBasicBlock* body;
        if (!newBlock(curBlock_, &body))
            return false;
        if (cond->isConstant() && cond->toConstant()->valueToBoolean()) {
            *afterLoop = nullptr;
            curBlock_->end(MGoto::New(alloc(), body));
        } else {
            if (!newBlockWithDepth(curBlock_, curBlock_->loopDepth() - 1, afterLoop))
                return false;
            curBlock_->end(MTest::New(alloc(), cond, body, *afterLoop));
        }
        curBlock_ = body;
        return true;
    }

  private:
    size_t popLoop()
    {
        size_t id = loopStack_.popCopy();
        MOZ_ASSERT(!unlabeledContinues_.has(id));
        breakableStack_.popBack();
        return id;
    }

    void fixupRedundantPhis(MBasicBlock* b)
    {
        for (size_t i = 0, depth = b->stackDepth(); i < depth; i++) {
            MDefinition* def = b->getSlot(i);
            if (def->isUnused())
                b->setSlot(i, def->toPhi()->getOperand(0));
        }
    }
    template <typename T>
    void fixupRedundantPhis(MBasicBlock* loopEntry, T& map)
    {
        if (!map.initialized())
            return;
        for (typename T::Enum e(map); !e.empty(); e.popFront()) {
            BlockVector& blocks = e.front().value();
            for (size_t i = 0; i < blocks.length(); i++) {
                if (blocks[i]->loopDepth() >= loopEntry->loopDepth())
                    fixupRedundantPhis(blocks[i]);
            }
        }
    }
    bool setLoopBackedge(MBasicBlock* loopEntry, MBasicBlock* backedge, MBasicBlock* afterLoop)
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
        if (afterLoop)
            fixupRedundantPhis(afterLoop);
        fixupRedundantPhis(loopEntry, labeledContinues_);
        fixupRedundantPhis(loopEntry, labeledBreaks_);
        fixupRedundantPhis(loopEntry, unlabeledContinues_);
        fixupRedundantPhis(loopEntry, unlabeledBreaks_);

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
    bool closeLoop(MBasicBlock* loopEntry, MBasicBlock* afterLoop)
    {
        size_t id = popLoop();
        if (!loopEntry) {
            MOZ_ASSERT(!afterLoop);
            MOZ_ASSERT(inDeadCode());
            MOZ_ASSERT(!unlabeledBreaks_.has(id));
            return true;
        }
        MOZ_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        MOZ_ASSERT_IF(afterLoop, afterLoop->loopDepth() == loopStack_.length());
        if (curBlock_) {
            MOZ_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            curBlock_->end(MGoto::New(alloc(), loopEntry));
            if (!setLoopBackedge(loopEntry, curBlock_, afterLoop))
                return false;
        }
        curBlock_ = afterLoop;
        if (curBlock_)
            mirGraph().moveBlockToEnd(curBlock_);
        return bindUnlabeledBreaks(id);
    }

    bool branchAndCloseDoWhileLoop(MDefinition* cond, MBasicBlock* loopEntry)
    {
        size_t id = popLoop();
        if (!loopEntry) {
            MOZ_ASSERT(inDeadCode());
            MOZ_ASSERT(!unlabeledBreaks_.has(id));
            return true;
        }
        MOZ_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        if (curBlock_) {
            MOZ_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            if (cond->isConstant()) {
                if (cond->toConstant()->valueToBoolean()) {
                    curBlock_->end(MGoto::New(alloc(), loopEntry));
                    if (!setLoopBackedge(loopEntry, curBlock_, nullptr))
                        return false;
                    curBlock_ = nullptr;
                } else {
                    MBasicBlock* afterLoop;
                    if (!newBlock(curBlock_, &afterLoop))
                        return false;
                    curBlock_->end(MGoto::New(alloc(), afterLoop));
                    curBlock_ = afterLoop;
                }
            } else {
                MBasicBlock* afterLoop;
                if (!newBlock(curBlock_, &afterLoop))
                    return false;
                curBlock_->end(MTest::New(alloc(), cond, loopEntry, afterLoop));
                if (!setLoopBackedge(loopEntry, curBlock_, afterLoop))
                    return false;
                curBlock_ = afterLoop;
            }
        }
        return bindUnlabeledBreaks(id);
    }

    bool bindContinues(size_t id, const LabelVector* maybeLabels)
    {
        bool createdJoinBlock = false;
        if (UnlabeledBlockMap::Ptr p = unlabeledContinues_.lookup(id)) {
            if (!bindBreaksOrContinues(&p->value(), &createdJoinBlock))
                return false;
            unlabeledContinues_.remove(p);
        }
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledContinues_, &createdJoinBlock);
    }

    bool bindLabeledBreaks(const LabelVector* maybeLabels)
    {
        bool createdJoinBlock = false;
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledBreaks_, &createdJoinBlock);
    }

    bool addBreak(uint32_t* maybeLabelId) {
        if (maybeLabelId)
            return addBreakOrContinue(*maybeLabelId, &labeledBreaks_);
        return addBreakOrContinue(breakableStack_.back(), &unlabeledBreaks_);
    }

    bool addContinue(uint32_t* maybeLabelId) {
        if (maybeLabelId)
            return addBreakOrContinue(*maybeLabelId, &labeledContinues_);
        return addBreakOrContinue(loopStack_.back(), &unlabeledContinues_);
    }

    bool startSwitch(size_t id, MDefinition* expr, int32_t low, int32_t high,
                     MBasicBlock** switchBlock)
    {
        if (!breakableStack_.append(id))
            return false;
        if (inDeadCode()) {
            *switchBlock = nullptr;
            return true;
        }
        curBlock_->end(MTableSwitch::New(alloc(), expr, low, high));
        *switchBlock = curBlock_;
        curBlock_ = nullptr;
        return true;
    }

    bool startSwitchCase(MBasicBlock* switchBlock, MBasicBlock** next)
    {
        if (!switchBlock) {
            *next = nullptr;
            return true;
        }
        if (!newBlock(switchBlock, next))
            return false;
        if (curBlock_) {
            curBlock_->end(MGoto::New(alloc(), *next));
            if (!(*next)->addPredecessor(alloc(), curBlock_))
                return false;
        }
        curBlock_ = *next;
        return true;
    }

    bool startSwitchDefault(MBasicBlock* switchBlock, BlockVector* cases, MBasicBlock** defaultBlock)
    {
        if (!startSwitchCase(switchBlock, defaultBlock))
            return false;
        if (!*defaultBlock)
            return true;
        mirGraph().moveBlockToEnd(*defaultBlock);
        return true;
    }

    bool joinSwitch(MBasicBlock* switchBlock, const BlockVector& cases, MBasicBlock* defaultBlock)
    {
        size_t id = breakableStack_.popCopy();
        if (!switchBlock)
            return true;
        MTableSwitch* mir = switchBlock->lastIns()->toTableSwitch();
        size_t defaultIndex;
        if (!mir->addDefault(defaultBlock, &defaultIndex))
            return false;
        for (unsigned i = 0; i < cases.length(); i++) {
            if (!cases[i]) {
                if (!mir->addCase(defaultIndex))
                    return false;
            } else {
                size_t caseIndex;
                if (!mir->addSuccessor(cases[i], &caseIndex))
                    return false;
                if (!mir->addCase(caseIndex))
                    return false;
            }
        }
        if (curBlock_) {
            MBasicBlock* next;
            if (!newBlock(curBlock_, &next))
                return false;
            curBlock_->end(MGoto::New(alloc(), next));
            curBlock_ = next;
        }
        return bindUnlabeledBreaks(id);
    }

    // Provides unique identifiers for internal uses in the control flow stacks;
    // these ids have to grow monotonically.
    unsigned nextId() { return nextId_++; }

    /************************************************************ DECODING ***/

    uint8_t        readU8()     { return decoder_.uncheckedReadU8(); }
    uint32_t       readU32()    { return decoder_.uncheckedReadU32(); }
    uint32_t       readVarU32() { return decoder_.uncheckedReadVarU32(); }
    int32_t        readI32()    { return decoder_.uncheckedReadI32(); }
    float          readF32()    { return decoder_.uncheckedReadF32(); }
    double         readF64()    { return decoder_.uncheckedReadF64(); }
    SimdConstant   readI32X4()  { return decoder_.uncheckedReadI32X4(); }
    SimdConstant   readF32X4()  { return decoder_.uncheckedReadF32X4(); }
    Expr           readOpcode() { return decoder_.uncheckedReadExpr(); }
    Expr           peekOpcode() { return decoder_.uncheckedPeekExpr(); }

    void readCallLineCol(uint32_t* line, uint32_t* column) {
        const SourceCoords& sc = func_.sourceCoords(lastReadCallSite_++);
        decoder_.assertCurrentIs(sc.offset);
        *line = sc.line;
        *column = sc.column;
    }

    void assertDebugCheckPoint() {
        MOZ_ASSERT(readOpcode() == Expr::DebugCheckPoint);
    }

    bool done() const { return decoder_.done(); }

    /*************************************************************************/
  private:
    bool newBlockWithDepth(MBasicBlock* pred, unsigned loopDepth, MBasicBlock** block)
    {
        *block = MBasicBlock::NewAsmJS(mirGraph(), info(), pred, MBasicBlock::NORMAL);
        if (!*block)
            return false;
        mirGraph().addBlock(*block);
        (*block)->setLoopDepth(loopDepth);
        return true;
    }

    bool newBlock(MBasicBlock* pred, MBasicBlock** block)
    {
        return newBlockWithDepth(pred, loopStack_.length(), block);
    }

    bool bindBreaksOrContinues(BlockVector* preds, bool* createdJoinBlock)
    {
        for (unsigned i = 0; i < preds->length(); i++) {
            MBasicBlock* pred = (*preds)[i];
            if (*createdJoinBlock) {
                pred->end(MGoto::New(alloc(), curBlock_));
                if (!curBlock_->addPredecessor(alloc(), pred))
                    return false;
            } else {
                MBasicBlock* next;
                if (!newBlock(pred, &next))
                    return false;
                pred->end(MGoto::New(alloc(), next));
                if (curBlock_) {
                    curBlock_->end(MGoto::New(alloc(), next));
                    if (!next->addPredecessor(alloc(), curBlock_))
                        return false;
                }
                curBlock_ = next;
                *createdJoinBlock = true;
            }
            MOZ_ASSERT(curBlock_->begin() == curBlock_->end());
            if (!mirGen_.ensureBallast())
                return false;
        }
        preds->clear();
        return true;
    }

    bool bindLabeledBreaksOrContinues(const LabelVector* maybeLabels, LabeledBlockMap* map,
                                      bool* createdJoinBlock)
    {
        if (!maybeLabels)
            return true;
        const LabelVector& labels = *maybeLabels;
        for (unsigned i = 0; i < labels.length(); i++) {
            if (LabeledBlockMap::Ptr p = map->lookup(labels[i])) {
                if (!bindBreaksOrContinues(&p->value(), createdJoinBlock))
                    return false;
                map->remove(p);
            }
            if (!mirGen_.ensureBallast())
                return false;
        }
        return true;
    }

    template <class Key, class Map>
    bool addBreakOrContinue(Key key, Map* map)
    {
        if (inDeadCode())
            return true;
        typename Map::AddPtr p = map->lookupForAdd(key);
        if (!p) {
            BlockVector empty;
            if (!map->add(p, key, Move(empty)))
                return false;
        }
        if (!p->value().append(curBlock_))
            return false;
        curBlock_ = nullptr;
        return true;
    }

    bool bindUnlabeledBreaks(size_t id)
    {
        bool createdJoinBlock = false;
        if (UnlabeledBlockMap::Ptr p = unlabeledBreaks_.lookup(id)) {
            if (!bindBreaksOrContinues(&p->value(), &createdJoinBlock))
                return false;
            unlabeledBreaks_.remove(p);
        }
        return true;
    }
};

// A Type or Undefined, implicitly constructed from an ExprType and usabled as
// an ExprType. Has two functions:
// - in debug, will ensure that the expected type and the actual type of some
// expressions match.
// - provides a way to mean "no type" in the context of expression statements,
// and will provoke assertions if we're trying to use an expected type when we
// don't have one.
class MaybeType
{
    Maybe<ExprType> maybe_;
    MaybeType() {}
  public:
    MOZ_IMPLICIT MaybeType(ExprType t) : maybe_() { maybe_.emplace(t); }
    static MaybeType Undefined() { return MaybeType(); }
    explicit operator bool() { return maybe_.isSome(); }
    MOZ_IMPLICIT operator ExprType() { return maybe_.value(); }
};

static bool
EmitLiteral(FunctionCompiler& f, ExprType type, MDefinition**def)
{
    switch (type) {
      case ExprType::I32: {
        int32_t val = f.readI32();
        *def = f.constant(Int32Value(val), MIRType_Int32);
        return true;
      }
      case ExprType::I64: {
        MOZ_CRASH("int64");
      }
      case ExprType::F32: {
        float val = f.readF32();
        *def = f.constant(Float32Value(val), MIRType_Float32);
        return true;
      }
      case ExprType::F64: {
        double val = f.readF64();
        *def = f.constant(DoubleValue(val), MIRType_Double);
        return true;
      }
      case ExprType::I32x4: {
        SimdConstant lit(f.readI32X4());
        *def = f.constant(lit, MIRType_Int32x4);
        return true;
      }
      case ExprType::F32x4: {
        SimdConstant lit(f.readF32X4());
        *def = f.constant(lit, MIRType_Float32x4);
        return true;
      }
      case ExprType::B32x4: {
        // Boolean vectors are stored as an Int vector with -1 / 0 lanes.
        SimdConstant lit(f.readI32X4());
        *def = f.constant(lit, MIRType_Bool32x4);
        return true;
      }
      case ExprType::Void: {
        break;
      }
    }
    MOZ_CRASH("unexpected literal type");
}

static bool
EmitGetLocal(FunctionCompiler& f, MaybeType type, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    *def = f.getLocalDef(slot);
    MOZ_ASSERT_IF(type, f.localType(slot) == type);
    return true;
}

static bool
EmitLoadGlobal(FunctionCompiler& f, MaybeType type, MDefinition** def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    *def = f.loadGlobalVar(global.globalDataOffset, global.isConst, ToMIRType(global.type));
    MOZ_ASSERT_IF(type, global.type == type);
    return true;
}

static bool EmitExpr(FunctionCompiler&, MaybeType, MDefinition**, LabelVector* = nullptr);
static bool EmitExprStmt(FunctionCompiler&, MDefinition**, LabelVector* = nullptr);
static bool EmitSimdBooleanLaneExpr(FunctionCompiler& f, MDefinition** def);

static bool
EmitLoadArray(FunctionCompiler& f, Scalar::Type scalarType, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    MDefinition* ptr;
    if (!EmitExpr(f, ExprType::I32, &ptr))
        return false;
    *def = f.loadHeap(scalarType, ptr, needsBoundsCheck);
    return true;
}

static bool
EmitStore(FunctionCompiler& f, Scalar::Type viewType, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());

    MDefinition* ptr;
    if (!EmitExpr(f, ExprType::I32, &ptr))
        return false;

    MDefinition* rhs = nullptr;
    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
        if (!EmitExpr(f, ExprType::I32, &rhs))
            return false;
        break;
      case Scalar::Float32:
        if (!EmitExpr(f, ExprType::F32, &rhs))
            return false;
        break;
      case Scalar::Float64:
        if (!EmitExpr(f, ExprType::F64, &rhs))
            return false;
        break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.storeHeap(viewType, ptr, rhs, needsBoundsCheck);
    *def = rhs;
    return true;
}

static bool
EmitStoreWithCoercion(FunctionCompiler& f, Scalar::Type rhsType, Scalar::Type viewType,
                      MDefinition **def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    MDefinition* ptr;
    if (!EmitExpr(f, ExprType::I32, &ptr))
        return false;

    MDefinition* rhs = nullptr;
    MDefinition* coerced = nullptr;
    if (rhsType == Scalar::Float32 && viewType == Scalar::Float64) {
        if (!EmitExpr(f, ExprType::F32, &rhs))
            return false;
        coerced = f.unary<MToDouble>(rhs);
    } else if (rhsType == Scalar::Float64 && viewType == Scalar::Float32) {
        if (!EmitExpr(f, ExprType::F64, &rhs))
            return false;
        coerced = f.unary<MToFloat32>(rhs);
    } else {
        MOZ_CRASH("unexpected coerced store");
    }

    f.storeHeap(viewType, ptr, coerced, needsBoundsCheck);
    *def = rhs;
    return true;
}

static bool
EmitSetLocal(FunctionCompiler& f, MaybeType expected, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    MDefinition* expr;
    ExprType actual = f.localType(slot);
    MOZ_ASSERT_IF(expected, actual == expected);
    if (!EmitExpr(f, actual, &expr))
        return false;
    f.assign(slot, expr);
    *def = expr;
    return true;
}

static bool
EmitStoreGlobal(FunctionCompiler& f, MaybeType type, MDefinition**def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    MOZ_ASSERT_IF(type, global.type == type);
    MDefinition* expr;
    if (!EmitExpr(f, global.type, &expr))
        return false;
    f.storeGlobalVar(global.globalDataOffset, expr);
    *def = expr;
    return true;
}

typedef bool IsMax;

static bool
EmitMathMinMax(FunctionCompiler& f, ExprType type, bool isMax, MDefinition** def)
{
    size_t numArgs = f.readU8();
    MOZ_ASSERT(numArgs >= 2);
    MDefinition* lastDef;
    if (!EmitExpr(f, type, &lastDef))
        return false;
    MIRType mirType = ToMIRType(type);
    for (size_t i = 1; i < numArgs; i++) {
        MDefinition* next;
        if (!EmitExpr(f, type, &next))
            return false;
        lastDef = f.minMax(lastDef, next, mirType, isMax);
    }
    *def = lastDef;
    return true;
}

static bool
EmitAtomicsLoad(FunctionCompiler& f, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    Scalar::Type viewType = Scalar::Type(f.readU8());
    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;
    *def = f.atomicLoadHeap(viewType, index, needsBoundsCheck);
    return true;
}

static bool
EmitAtomicsStore(FunctionCompiler& f, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    Scalar::Type viewType = Scalar::Type(f.readU8());
    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;
    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    f.atomicStoreHeap(viewType, index, value, needsBoundsCheck);
    *def = value;
    return true;
}

static bool
EmitAtomicsBinOp(FunctionCompiler& f, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    Scalar::Type viewType = Scalar::Type(f.readU8());
    js::jit::AtomicOp op = js::jit::AtomicOp(f.readU8());
    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;
    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    *def = f.atomicBinopHeap(op, viewType, index, value, needsBoundsCheck);
    return true;
}

static bool
EmitAtomicsCompareExchange(FunctionCompiler& f, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    Scalar::Type viewType = Scalar::Type(f.readU8());
    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;
    MDefinition* oldValue;
    if (!EmitExpr(f, ExprType::I32, &oldValue))
        return false;
    MDefinition* newValue;
    if (!EmitExpr(f, ExprType::I32, &newValue))
        return false;
    *def = f.atomicCompareExchangeHeap(viewType, index, oldValue, newValue, needsBoundsCheck);
    return true;
}

static bool
EmitAtomicsExchange(FunctionCompiler& f, MDefinition** def)
{
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    Scalar::Type viewType = Scalar::Type(f.readU8());
    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;
    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    *def = f.atomicExchangeHeap(viewType, index, value, needsBoundsCheck);
    return true;
}

static bool
EmitCallArgs(FunctionCompiler& f, const Sig& sig, FunctionCompiler::Call* call)
{
    f.startCallArgs(call);
    for (unsigned i = 0; i < sig.args().length(); i++) {
        MDefinition *arg = nullptr;
        switch (sig.arg(i)) {
          case ValType::I32:    if (!EmitExpr(f, ExprType::I32, &arg))   return false; break;
          case ValType::I64:    MOZ_CRASH("int64");
          case ValType::F32:    if (!EmitExpr(f, ExprType::F32, &arg))   return false; break;
          case ValType::F64:    if (!EmitExpr(f, ExprType::F64, &arg))   return false; break;
          case ValType::I32x4:  if (!EmitExpr(f, ExprType::I32x4, &arg)) return false; break;
          case ValType::F32x4:  if (!EmitExpr(f, ExprType::F32x4, &arg)) return false; break;
          case ValType::B32x4:  if (!EmitExpr(f, ExprType::B32x4, &arg)) return false; break;
        }
        if (!f.passArg(arg, sig.arg(i), call))
            return false;
    }
    f.finishCallArgs(call);
    return true;
}

static bool
EmitInternalCall(FunctionCompiler& f, MaybeType ret, MDefinition** def)
{
    uint32_t funcIndex = f.readU32();

    const Sig& sig = f.mg().funcSig(funcIndex);
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret, sig.ret() == ret);

    uint32_t lineno, column;
    f.readCallLineCol(&lineno, &column);

    FunctionCompiler::Call call(f, lineno, column);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.internalCall(sig, funcIndex, call, def);
}

static bool
EmitFuncPtrCall(FunctionCompiler& f, MaybeType ret, MDefinition** def)
{
    uint32_t mask = f.readU32();
    uint32_t globalDataOffset = f.readU32();
    uint32_t sigIndex = f.readU32();

    const Sig& sig = f.mg().sig(sigIndex);
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret, sig.ret() == ret);

    uint32_t lineno, column;
    f.readCallLineCol(&lineno, &column);

    MDefinition *index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    FunctionCompiler::Call call(f, lineno, column);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.funcPtrCall(sig, mask, globalDataOffset, index, call, def);
}

static bool
EmitFFICall(FunctionCompiler& f, MaybeType ret, MDefinition** def)
{
    uint32_t importIndex = f.readU32();

    uint32_t lineno, column;
    f.readCallLineCol(&lineno, &column);

    const ModuleImportGeneratorData& import = f.mg().import(importIndex);
    const Sig& sig = *import.sig;
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret, sig.ret() == ret);

    FunctionCompiler::Call call(f, lineno, column);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.ffiCall(import.globalDataOffset, call, sig.ret(), def);
}

static bool
EmitF32MathBuiltinCall(FunctionCompiler& f, Expr f32, MDefinition** def)
{
    MOZ_ASSERT(f32 == Expr::F32Ceil || f32 == Expr::F32Floor);

    uint32_t lineno, column;
    f.readCallLineCol(&lineno, &column);

    FunctionCompiler::Call call(f, lineno, column);
    f.startCallArgs(&call);

    MDefinition* firstArg;
    if (!EmitExpr(f, ExprType::F32, &firstArg) || !f.passArg(firstArg, ValType::F32, &call))
        return false;

    f.finishCallArgs(&call);

    SymbolicAddress callee = f32 == Expr::F32Ceil ? SymbolicAddress::CeilF : SymbolicAddress::FloorF;
    return f.builtinCall(callee, call, ValType::F32, def);
}

static bool
EmitF64MathBuiltinCall(FunctionCompiler& f, Expr f64, MDefinition** def)
{
    uint32_t lineno, column;
    f.readCallLineCol(&lineno, &column);

    FunctionCompiler::Call call(f, lineno, column);
    f.startCallArgs(&call);

    MDefinition* firstArg;
    if (!EmitExpr(f, ExprType::F64, &firstArg) || !f.passArg(firstArg, ValType::F64, &call))
        return false;

    if (f64 == Expr::F64Pow || f64 == Expr::F64Atan2) {
        MDefinition* secondArg;
        if (!EmitExpr(f, ExprType::F64, &secondArg) || !f.passArg(secondArg, ValType::F64, &call))
            return false;
    }

    SymbolicAddress callee;
    switch (f64) {
      case Expr::F64Ceil:  callee = SymbolicAddress::CeilD; break;
      case Expr::F64Floor: callee = SymbolicAddress::FloorD; break;
      case Expr::F64Sin:   callee = SymbolicAddress::SinD; break;
      case Expr::F64Cos:   callee = SymbolicAddress::CosD; break;
      case Expr::F64Tan:   callee = SymbolicAddress::TanD; break;
      case Expr::F64Asin:  callee = SymbolicAddress::ASinD; break;
      case Expr::F64Acos:  callee = SymbolicAddress::ACosD; break;
      case Expr::F64Atan:  callee = SymbolicAddress::ATanD; break;
      case Expr::F64Exp:   callee = SymbolicAddress::ExpD; break;
      case Expr::F64Log:   callee = SymbolicAddress::LogD; break;
      case Expr::F64Pow:   callee = SymbolicAddress::PowD; break;
      case Expr::F64Atan2: callee = SymbolicAddress::ATan2D; break;
      default: MOZ_CRASH("unexpected double math builtin callee");
    }

    f.finishCallArgs(&call);

    return f.builtinCall(callee, call, ValType::F64, def);
}

static bool
EmitSimdUnary(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MSimdUnaryArith::Operation op = MSimdUnaryArith::Operation(f.readU8());
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.unarySimd(in, op, ToMIRType(type));
    return true;
}

template<class OpKind>
inline bool
EmitBinarySimdGuts(FunctionCompiler& f, ExprType type, OpKind op, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    *def = f.binarySimd(lhs, rhs, op, ToMIRType(type));
    return true;
}

static bool
EmitSimdBinaryArith(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MSimdBinaryArith::Operation op = MSimdBinaryArith::Operation(f.readU8());
    return EmitBinarySimdGuts(f, type, op, def);
}

static bool
EmitSimdBinaryBitwise(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MSimdBinaryBitwise::Operation op = MSimdBinaryBitwise::Operation(f.readU8());
    return EmitBinarySimdGuts(f, type, op, def);
}

static bool
EmitSimdBinaryComp(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MSimdBinaryComp::Operation op = MSimdBinaryComp::Operation(f.readU8());
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    *def = f.binarySimd<MSimdBinaryComp>(lhs, rhs, op);
    return true;
}

static bool
EmitSimdBinaryShift(FunctionCompiler& f, MDefinition** def)
{
    MSimdShift::Operation op = MSimdShift::Operation(f.readU8());
    MDefinition* lhs;
    if (!EmitExpr(f, ExprType::I32x4, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, ExprType::I32, &rhs))
        return false;
    *def = f.binarySimd<MSimdShift>(lhs, rhs, op);
    return true;
}

static ExprType
SimdToLaneType(ExprType type)
{
    switch (type) {
      case ExprType::I32x4:  return ExprType::I32;
      case ExprType::F32x4:  return ExprType::F32;
      case ExprType::B32x4:  return ExprType::I32; // Boolean lanes are Int32 in asm.
      case ExprType::I32:
      case ExprType::I64:
      case ExprType::F32:
      case ExprType::F64:
      case ExprType::Void:;
    }
    MOZ_CRASH("bad simd type");
}

static bool
EmitExtractLane(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* vec;
    if (!EmitExpr(f, type, &vec))
        return false;

    MDefinition* laneDef;
    if (!EmitExpr(f, ExprType::I32, &laneDef))
        return false;

    if (!laneDef) {
        *def = nullptr;
        return true;
    }

    MOZ_ASSERT(laneDef->isConstant());
    int32_t laneLit = laneDef->toConstant()->value().toInt32();
    MOZ_ASSERT(laneLit < 4);
    SimdLane lane = SimdLane(laneLit);

    *def = f.extractSimdElement(lane, vec, ToMIRType(SimdToLaneType(type)));
    return true;
}

static bool
EmitSimdReplaceLane(FunctionCompiler& f, ExprType simdType, MDefinition** def)
{
    MDefinition* vector;
    if (!EmitExpr(f, simdType, &vector))
        return false;

    MDefinition* laneDef;
    if (!EmitExpr(f, ExprType::I32, &laneDef))
        return false;

    SimdLane lane;
    if (laneDef) {
        MOZ_ASSERT(laneDef->isConstant());
        int32_t laneLit = laneDef->toConstant()->value().toInt32();
        MOZ_ASSERT(laneLit < 4);
        lane = SimdLane(laneLit);
    } else {
        lane = SimdLane(-1);
    }

    MDefinition* scalar;
    if (IsSimdBoolType(simdType)) {
        if (!EmitSimdBooleanLaneExpr(f, &scalar))
            return false;
    } else if (!EmitExpr(f, SimdToLaneType(simdType), &scalar)) {
            return false;
    }
    *def = f.insertElementSimd(vector, scalar, lane, ToMIRType(simdType));
    return true;
}

template<class T>
inline bool
EmitSimdCast(FunctionCompiler& f, ExprType fromType, ExprType toType, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, fromType, &in))
        return false;
    *def = f.convertSimd<T>(in, ToMIRType(fromType), ToMIRType(toType));
    return true;
}

static bool
EmitSimdSwizzle(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;

    uint8_t lanes[4];
    for (unsigned i = 0; i < 4; i++)
        lanes[i] = f.readU8();

    *def = f.swizzleSimd(in, lanes[0], lanes[1], lanes[2], lanes[3], ToMIRType(type));
    return true;
}

static bool
EmitSimdShuffle(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;

    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;

    uint8_t lanes[4];
    for (unsigned i = 0; i < 4; i++)
        lanes[i] = f.readU8();

    *def = f.shuffleSimd(lhs, rhs, lanes[0], lanes[1], lanes[2], lanes[3], ToMIRType(type));
    return true;
}

static bool
EmitSimdLoad(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    uint8_t numElems = f.readU8();

    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    *def = f.loadSimdHeap(viewType, index, needsBoundsCheck, numElems);
    return true;
}

static bool
EmitSimdStore(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());
    NeedsBoundsCheck needsBoundsCheck = NeedsBoundsCheck(f.readU8());
    uint8_t numElems = f.readU8();

    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    MDefinition* vec;
    if (!EmitExpr(f, type, &vec))
        return false;

    f.storeSimdHeap(viewType, index, vec, needsBoundsCheck, numElems);
    *def = vec;
    return true;
}

static bool
EmitSimdSelect(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* mask;
    MDefinition* defs[2];

    // The mask is a boolean vector for elementwise select.
    if (!EmitExpr(f, ExprType::B32x4, &mask))
        return false;

    if (!EmitExpr(f, type, &defs[0]) || !EmitExpr(f, type, &defs[1]))
        return false;
    *def = f.selectSimd(mask, defs[0], defs[1], ToMIRType(type));
    return true;
}

static bool
EmitSimdAllTrue(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.simdAllTrue(in);
    return true;
}

static bool
EmitSimdAnyTrue(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.simdAnyTrue(in);
    return true;
}

static bool
EmitSimdSplat(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, SimdToLaneType(type), &in))
        return false;
    *def = f.splatSimd(in, ToMIRType(type));
    return true;
}

static bool
EmitSimdBooleanSplat(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitSimdBooleanLaneExpr(f, &in))
        return false;
    *def = f.splatSimd(in, MIRType_Bool32x4);
    return true;
}

static bool
EmitSimdCtor(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    switch (type) {
      case ExprType::I32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitExpr(f, ExprType::I32, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Int32x4);
        return true;
      }
      case ExprType::F32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitExpr(f, ExprType::F32, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Float32x4);
        return true;
      }
      case ExprType::B32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitSimdBooleanLaneExpr(f, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Bool32x4);
        return true;
      }
      case ExprType::I32:
      case ExprType::I64:
      case ExprType::F32:
      case ExprType::F64:
      case ExprType::Void:
        break;
    }
    MOZ_CRASH("unexpected SIMD type");
}

template<class T>
static bool
EmitUnary(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.unary<T>(in);
    return true;
}

template<class T>
static bool
EmitUnaryMir(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.unary<T>(in, ToMIRType(type));
    return true;
}

static bool
EmitTernary(FunctionCompiler& f, MaybeType type, MDefinition** def)
{
    MDefinition* cond;
    if (!EmitExpr(f, ExprType::I32, &cond))
        return false;

    MBasicBlock* thenBlock = nullptr;
    MBasicBlock* elseBlock = nullptr;
    if (!f.branchAndStartThen(cond, &thenBlock, &elseBlock))
        return false;

    MDefinition* ifTrue;
    if (!EmitExpr(f, type, &ifTrue))
        return false;

    BlockVector thenBlocks;
    if (!f.appendThenBlock(&thenBlocks))
        return false;

    f.pushPhiInput(ifTrue);

    f.switchToElse(elseBlock);

    MDefinition* ifFalse;
    if (!EmitExpr(f, type, &ifFalse))
        return false;

    f.pushPhiInput(ifFalse);

    if (!f.joinIfElse(thenBlocks))
        return false;

    MOZ_ASSERT(ifTrue->type() == ifFalse->type(), "both sides of a ternary have the same type");

    *def = f.popPhiOutput();
    return true;
}

static bool
EmitMultiply(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.mul(lhs, rhs, mirType, mirType == MIRType_Int32 ? MMul::Integer : MMul::Normal);
    return true;
}

typedef bool IsAdd;

static bool
EmitAddOrSub(FunctionCompiler& f, ExprType type, bool isAdd, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = isAdd ? f.binary<MAdd>(lhs, rhs, mirType) : f.binary<MSub>(lhs, rhs, mirType);
    return true;
}

typedef bool IsUnsigned;
typedef bool IsDiv;

static bool
EmitDivOrMod(FunctionCompiler& f, ExprType type, bool isDiv, bool isUnsigned, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    *def = isDiv
           ? f.div(lhs, rhs, ToMIRType(type), isUnsigned)
           : f.mod(lhs, rhs, ToMIRType(type), isUnsigned);
    return true;
}

static bool
EmitDivOrMod(FunctionCompiler& f, ExprType type, bool isDiv, MDefinition** def)
{
    MOZ_ASSERT(type != ExprType::I32, "int div or mod must precise signedness");
    return EmitDivOrMod(f, type, isDiv, false, def);
}

static bool
EmitComparison(FunctionCompiler& f, Expr expr, MDefinition** def)
{
    MDefinition *lhs, *rhs;
    MCompare::CompareType compareType;
    switch (expr) {
      case Expr::I32Eq:
      case Expr::I32Ne:
      case Expr::I32LeS:
      case Expr::I32LtS:
      case Expr::I32LeU:
      case Expr::I32LtU:
      case Expr::I32GeS:
      case Expr::I32GtS:
      case Expr::I32GeU:
      case Expr::I32GtU:
        if (!EmitExpr(f, ExprType::I32, &lhs) || !EmitExpr(f, ExprType::I32, &rhs))
            return false;

        switch (expr) {
          case Expr::I32LeS: case Expr::I32LtS: case Expr::I32GeS: case Expr::I32GtS:
          case Expr::I32Eq: case Expr::I32Ne:
            compareType = MCompare::Compare_Int32; break;
          case Expr::I32GeU: case Expr::I32GtU: case Expr::I32LeU: case Expr::I32LtU:
            compareType = MCompare::Compare_UInt32; break;
          default: MOZ_CRASH("impossibru opcode");
        }
        break;
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Le:
      case Expr::F32Lt:
      case Expr::F32Ge:
      case Expr::F32Gt:
        if (!EmitExpr(f, ExprType::F32, &lhs) || !EmitExpr(f, ExprType::F32, &rhs))
            return false;
        compareType = MCompare::Compare_Float32;
        break;
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Le:
      case Expr::F64Lt:
      case Expr::F64Ge:
      case Expr::F64Gt:
        if (!EmitExpr(f, ExprType::F64, &lhs) || !EmitExpr(f, ExprType::F64, &rhs))
            return false;
        compareType = MCompare::Compare_Double;
        break;
      default: MOZ_CRASH("unexpected comparison opcode");
    }

    JSOp compareOp;
    switch (expr) {
      case Expr::I32Eq:
      case Expr::F32Eq:
      case Expr::F64Eq:
        compareOp = JSOP_EQ;
        break;
      case Expr::I32Ne:
      case Expr::F32Ne:
      case Expr::F64Ne:
        compareOp = JSOP_NE;
        break;
      case Expr::I32LeS:
      case Expr::I32LeU:
      case Expr::F32Le:
      case Expr::F64Le:
        compareOp = JSOP_LE;
        break;
      case Expr::I32LtS:
      case Expr::I32LtU:
      case Expr::F32Lt:
      case Expr::F64Lt:
        compareOp = JSOP_LT;
        break;
      case Expr::I32GeS:
      case Expr::I32GeU:
      case Expr::F32Ge:
      case Expr::F64Ge:
        compareOp = JSOP_GE;
        break;
      case Expr::I32GtS:
      case Expr::I32GtU:
      case Expr::F32Gt:
      case Expr::F64Gt:
        compareOp = JSOP_GT;
        break;
      default: MOZ_CRASH("unexpected comparison opcode");
    }

    *def = f.compare(lhs, rhs, compareOp, compareType);
    return true;
}

template<class T>
static bool
EmitBitwise(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, ExprType::I32, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, ExprType::I32, &rhs))
        return false;
    *def = f.bitwise<T>(lhs, rhs);
    return true;
}

template<>
bool
EmitBitwise<MBitNot>(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, ExprType::I32, &in))
        return false;
    *def = f.bitwise<MBitNot>(in);
    return true;
}

// Emit an I32 expression and then convert it to a boolean SIMD lane value, i.e. -1 or 0.
static bool
EmitSimdBooleanLaneExpr(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* i32;
    if (!EmitExpr(f, ExprType::I32, &i32))
        return false;
    // Now compute !i32 - 1 to force the value range into {0, -1}.
    MDefinition* noti32 = f.unary<MNot>(i32);
    *def = f.binary<MSub>(noti32, f.constant(Int32Value(1), MIRType_Int32), MIRType_Int32);
    return true;
}

static bool
EmitInterruptCheck(FunctionCompiler& f)
{
    unsigned lineno = f.readU32();
    unsigned column = f.readU32();
    f.addInterruptCheck(lineno, column);
    return true;
}

static bool
EmitInterruptCheckLoop(FunctionCompiler& f)
{
    if (!EmitInterruptCheck(f))
        return false;
    MDefinition* _;
    return EmitExprStmt(f, &_);
}

static bool
EmitWhile(FunctionCompiler& f, const LabelVector* maybeLabels)
{
    size_t headId = f.nextId();

    MBasicBlock* loopEntry;
    if (!f.startPendingLoop(headId, &loopEntry))
        return false;

    MDefinition* condDef;
    if (!EmitExpr(f, ExprType::I32, &condDef))
        return false;

    MBasicBlock* afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop))
        return false;

    MDefinition* _;
    if (!EmitExprStmt(f, &_))
        return false;

    if (!f.bindContinues(headId, maybeLabels))
        return false;

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
EmitFor(FunctionCompiler& f, Expr expr, const LabelVector* maybeLabels)
{
    MOZ_ASSERT(expr == Expr::ForInitInc || expr == Expr::ForInitNoInc ||
               expr == Expr::ForNoInitInc || expr == Expr::ForNoInitNoInc);
    size_t headId = f.nextId();

    if (expr == Expr::ForInitInc || expr == Expr::ForInitNoInc) {
        MDefinition* _;
        if (!EmitExprStmt(f, &_))
            return false;
    }

    MBasicBlock* loopEntry;
    if (!f.startPendingLoop(headId, &loopEntry))
        return false;

    MDefinition* condDef;
    if (!EmitExpr(f, ExprType::I32, &condDef))
        return false;

    MBasicBlock* afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop))
        return false;

    MDefinition* _;
    if (!EmitExprStmt(f, &_))
        return false;

    if (!f.bindContinues(headId, maybeLabels))
        return false;

    if (expr == Expr::ForInitInc || expr == Expr::ForNoInitInc) {
        MDefinition* _;
        if (!EmitExprStmt(f, &_))
            return false;
    }

    f.assertDebugCheckPoint();

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
EmitDoWhile(FunctionCompiler& f, const LabelVector* maybeLabels)
{
    size_t headId = f.nextId();

    MBasicBlock* loopEntry;
    if (!f.startPendingLoop(headId, &loopEntry))
        return false;

    MDefinition* _;
    if (!EmitExprStmt(f, &_))
        return false;

    if (!f.bindContinues(headId, maybeLabels))
        return false;

    MDefinition* condDef;
    if (!EmitExpr(f, ExprType::I32, &condDef))
        return false;

    return f.branchAndCloseDoWhileLoop(condDef, loopEntry);
}

static bool
EmitLabel(FunctionCompiler& f, LabelVector* maybeLabels)
{
    uint32_t labelId = f.readU32();

    if (maybeLabels) {
        if (!maybeLabels->append(labelId))
            return false;
        MDefinition* _;
        return EmitExprStmt(f, &_, maybeLabels);
    }

    LabelVector labels;
    if (!labels.append(labelId))
        return false;

    MDefinition* _;
    if (!EmitExprStmt(f, &_, &labels))
        return false;

    return f.bindLabeledBreaks(&labels);
}

typedef bool HasElseBlock;

static bool
EmitIfElse(FunctionCompiler& f, bool hasElse)
{
    // Handle if/else-if chains using iteration instead of recursion. This
    // avoids blowing the C stack quota for long if/else-if chains and also
    // creates fewer MBasicBlocks at join points (by creating one join block
    // for the entire if/else-if chain).
    BlockVector thenBlocks;

  recurse:
    MDefinition* condition;
    if (!EmitExpr(f, ExprType::I32, &condition))
        return false;

    MBasicBlock* thenBlock = nullptr;
    MBasicBlock* elseOrJoinBlock = nullptr;
    if (!f.branchAndStartThen(condition, &thenBlock, &elseOrJoinBlock))
        return false;

    MDefinition* _;
    if (!EmitExprStmt(f, &_))
        return false;

    if (!f.appendThenBlock(&thenBlocks))
        return false;

    if (hasElse) {
        f.switchToElse(elseOrJoinBlock);

        Expr nextStmt = f.peekOpcode();
        if (nextStmt == Expr::If || nextStmt == Expr::IfElse) {
            hasElse = nextStmt == Expr::IfElse;
            JS_ALWAYS_TRUE(f.readOpcode() == nextStmt);
            goto recurse;
        }

        MDefinition* _;
        if (!EmitExprStmt(f, &_))
            return false;

        return f.joinIfElse(thenBlocks);
    } else {
        return f.joinIf(thenBlocks, elseOrJoinBlock);
    }
}

static bool
EmitTableSwitch(FunctionCompiler& f)
{
    bool hasDefault = f.readU8();
    int32_t low = f.readI32();
    int32_t high = f.readI32();
    uint32_t numCases = f.readU32();

    MDefinition* exprDef;
    if (!EmitExpr(f, ExprType::I32, &exprDef))
        return false;

    // Switch with no cases
    if (!hasDefault && numCases == 0)
        return true;

    BlockVector cases;
    if (!cases.resize(high - low + 1))
        return false;

    MBasicBlock* switchBlock;
    if (!f.startSwitch(f.nextId(), exprDef, low, high, &switchBlock))
        return false;

    while (numCases--) {
        int32_t caseValue = f.readI32();
        MOZ_ASSERT(caseValue >= low && caseValue <= high);
        unsigned caseIndex = caseValue - low;
        if (!f.startSwitchCase(switchBlock, &cases[caseIndex]))
            return false;
        MDefinition* _;
        if (!EmitExprStmt(f, &_))
            return false;
    }

    MBasicBlock* defaultBlock;
    if (!f.startSwitchDefault(switchBlock, &cases, &defaultBlock))
        return false;

    MDefinition* _;
    if (hasDefault && !EmitExprStmt(f, &_))
        return false;

    return f.joinSwitch(switchBlock, cases, defaultBlock);
}

static bool
EmitRet(FunctionCompiler& f)
{
    ExprType ret = f.sig().ret();

    if (IsVoid(ret)) {
        f.returnVoid();
        return true;
    }

    MDefinition *def = nullptr;
    if (!EmitExpr(f, ret, &def))
        return false;
    f.returnExpr(def);
    return true;
}

static bool
EmitBlock(FunctionCompiler& f, MaybeType type, MDefinition** def)
{
    size_t numStmt = f.readU32();
    for (size_t i = 1; i < numStmt; i++) {
        // Fine to clobber def, we only want the last use.
        if (!EmitExprStmt(f, def))
            return false;
    }
    if (numStmt && !EmitExpr(f, type, def))
        return false;
    f.assertDebugCheckPoint();
    return true;
}

typedef bool HasLabel;

static bool
EmitContinue(FunctionCompiler& f, bool hasLabel)
{
    if (!hasLabel)
        return f.addContinue(nullptr);
    uint32_t labelId = f.readU32();
    return f.addContinue(&labelId);
}

static bool
EmitBreak(FunctionCompiler& f, bool hasLabel)
{
    if (!hasLabel)
        return f.addBreak(nullptr);
    uint32_t labelId = f.readU32();
    return f.addBreak(&labelId);
}

static bool
EmitExpr(FunctionCompiler& f, MaybeType type, MDefinition** def, LabelVector* maybeLabels)
{
    if (!f.mirGen().ensureBallast())
        return false;

    switch (Expr op = f.readOpcode()) {
      case Expr::Nop:
        return true;
      case Expr::Block:
        return EmitBlock(f, type, def);
      case Expr::If:
      case Expr::IfElse:
        return EmitIfElse(f, HasElseBlock(op == Expr::IfElse));
      case Expr::TableSwitch:
        return EmitTableSwitch(f);
      case Expr::Ternary:
        return EmitTernary(f, type, def);
      case Expr::While:
        return EmitWhile(f, maybeLabels);
      case Expr::DoWhile:
        return EmitDoWhile(f, maybeLabels);
      case Expr::ForInitInc:
      case Expr::ForInitNoInc:
      case Expr::ForNoInitNoInc:
      case Expr::ForNoInitInc:
        return EmitFor(f, op, maybeLabels);
      case Expr::Label:
        return EmitLabel(f, maybeLabels);
      case Expr::Continue:
        return EmitContinue(f, HasLabel(false));
      case Expr::ContinueLabel:
        return EmitContinue(f, HasLabel(true));
      case Expr::Break:
        return EmitBreak(f, HasLabel(false));
      case Expr::BreakLabel:
        return EmitBreak(f, HasLabel(true));
      case Expr::Return:
        return EmitRet(f);
      case Expr::CallInternal:
        return EmitInternalCall(f, type, def);
      case Expr::CallIndirect:
        return EmitFuncPtrCall(f, type, def);
      case Expr::CallImport:
        return EmitFFICall(f, type, def);
      case Expr::AtomicsFence:
        f.memoryBarrier(MembarFull);
        return true;
      case Expr::InterruptCheckHead:
        return EmitInterruptCheck(f);
      case Expr::InterruptCheckLoop:
        return EmitInterruptCheckLoop(f);
      // Common
      case Expr::GetLocal:
        return EmitGetLocal(f, type, def);
      case Expr::SetLocal:
        return EmitSetLocal(f, type, def);
      case Expr::LoadGlobal:
        return EmitLoadGlobal(f, type, def);
      case Expr::StoreGlobal:
        return EmitStoreGlobal(f, type, def);
      case Expr::Id:
        return EmitExpr(f, type, def);
      // I32
      case Expr::I32Const:
        return EmitLiteral(f, ExprType::I32, def);
      case Expr::I32Add:
        return EmitAddOrSub(f, ExprType::I32, IsAdd(true), def);
      case Expr::I32Sub:
        return EmitAddOrSub(f, ExprType::I32, IsAdd(false), def);
      case Expr::I32Mul:
        return EmitMultiply(f, ExprType::I32, def);
      case Expr::I32DivS:
      case Expr::I32DivU:
        return EmitDivOrMod(f, ExprType::I32, IsDiv(true), IsUnsigned(op == Expr::I32DivU), def);
      case Expr::I32RemS:
      case Expr::I32RemU:
        return EmitDivOrMod(f, ExprType::I32, IsDiv(false), IsUnsigned(op == Expr::I32RemU), def);
      case Expr::I32Min:
        return EmitMathMinMax(f, ExprType::I32, IsMax(false), def);
      case Expr::I32Max:
        return EmitMathMinMax(f, ExprType::I32, IsMax(true), def);
      case Expr::I32Not:
        return EmitUnary<MNot>(f, ExprType::I32, def);
      case Expr::I32SConvertF32:
        return EmitUnary<MTruncateToInt32>(f, ExprType::F32, def);
      case Expr::I32SConvertF64:
        return EmitUnary<MTruncateToInt32>(f, ExprType::F64, def);
      case Expr::I32Clz:
        return EmitUnary<MClz>(f, ExprType::I32, def);
      case Expr::I32Abs:
        return EmitUnaryMir<MAbs>(f, ExprType::I32, def);
      case Expr::I32Neg:
        return EmitUnaryMir<MAsmJSNeg>(f, ExprType::I32, def);
      case Expr::I32Ior:
        return EmitBitwise<MBitOr>(f, def);
      case Expr::I32And:
        return EmitBitwise<MBitAnd>(f, def);
      case Expr::I32Xor:
        return EmitBitwise<MBitXor>(f, def);
      case Expr::I32Shl:
        return EmitBitwise<MLsh>(f, def);
      case Expr::I32ShrS:
        return EmitBitwise<MRsh>(f, def);
      case Expr::I32ShrU:
        return EmitBitwise<MUrsh>(f, def);
      case Expr::I32BitNot:
        return EmitBitwise<MBitNot>(f, def);
      case Expr::I32LoadMem8S:
        return EmitLoadArray(f, Scalar::Int8, def);
      case Expr::I32LoadMem8U:
        return EmitLoadArray(f, Scalar::Uint8, def);
      case Expr::I32LoadMem16S:
        return EmitLoadArray(f, Scalar::Int16, def);
      case Expr::I32LoadMem16U:
        return EmitLoadArray(f, Scalar::Uint16, def);
      case Expr::I32LoadMem:
        return EmitLoadArray(f, Scalar::Int32, def);
      case Expr::I32StoreMem8:
        return EmitStore(f, Scalar::Int8, def);
      case Expr::I32StoreMem16:
        return EmitStore(f, Scalar::Int16, def);
      case Expr::I32StoreMem:
        return EmitStore(f, Scalar::Int32, def);
      case Expr::I32Eq:
      case Expr::I32Ne:
      case Expr::I32LtS:
      case Expr::I32LeS:
      case Expr::I32GtS:
      case Expr::I32GeS:
      case Expr::I32LtU:
      case Expr::I32LeU:
      case Expr::I32GtU:
      case Expr::I32GeU:
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return EmitComparison(f, op, def);
      case Expr::I32AtomicsCompareExchange:
        return EmitAtomicsCompareExchange(f, def);
      case Expr::I32AtomicsExchange:
        return EmitAtomicsExchange(f, def);
      case Expr::I32AtomicsLoad:
        return EmitAtomicsLoad(f, def);
      case Expr::I32AtomicsStore:
        return EmitAtomicsStore(f, def);
      case Expr::I32AtomicsBinOp:
        return EmitAtomicsBinOp(f, def);
      case Expr::I32I32X4ExtractLane:
        return EmitExtractLane(f, ExprType::I32x4, def);
      case Expr::I32B32X4ExtractLane:
        return EmitExtractLane(f, ExprType::B32x4, def);
      case Expr::I32B32X4AllTrue:
        return EmitSimdAllTrue(f, ExprType::B32x4, def);
      case Expr::I32B32X4AnyTrue:
        return EmitSimdAnyTrue(f, ExprType::B32x4, def);
      // F32
      case Expr::F32Const:
        return EmitLiteral(f, ExprType::F32, def);
      case Expr::F32Add:
        return EmitAddOrSub(f, ExprType::F32, IsAdd(true), def);
      case Expr::F32Sub:
        return EmitAddOrSub(f, ExprType::F32, IsAdd(false), def);
      case Expr::F32Mul:
        return EmitMultiply(f, ExprType::F32, def);
      case Expr::F32Div:
        return EmitDivOrMod(f, ExprType::F32, IsDiv(true), def);
      case Expr::F32Min:
        return EmitMathMinMax(f, ExprType::F32, IsMax(false), def);
      case Expr::F32Max:
        return EmitMathMinMax(f, ExprType::F32, IsMax(true), def);
      case Expr::F32Neg:
        return EmitUnaryMir<MAsmJSNeg>(f, ExprType::F32, def);
      case Expr::F32Abs:
        return EmitUnaryMir<MAbs>(f, ExprType::F32, def);
      case Expr::F32Sqrt:
        return EmitUnaryMir<MSqrt>(f, ExprType::F32, def);
      case Expr::F32Ceil:
      case Expr::F32Floor:
        return EmitF32MathBuiltinCall(f, op, def);
      case Expr::F32FromF64:
        return EmitUnary<MToFloat32>(f, ExprType::F64, def);
      case Expr::F32FromS32:
        return EmitUnary<MToFloat32>(f, ExprType::I32, def);
      case Expr::F32FromU32:
        return EmitUnary<MAsmJSUnsignedToFloat32>(f, ExprType::I32, def);
      case Expr::F32LoadMem:
        return EmitLoadArray(f, Scalar::Float32, def);
      case Expr::F32StoreMem:
        return EmitStore(f, Scalar::Float32, def);
      case Expr::F32StoreMemF64:
        return EmitStoreWithCoercion(f, Scalar::Float32, Scalar::Float64, def);
      case Expr::F32F32X4ExtractLane:
        return EmitExtractLane(f, ExprType::F32x4, def);
      // F64
      case Expr::F64Const:
        return EmitLiteral(f, ExprType::F64, def);
      case Expr::F64Add:
        return EmitAddOrSub(f, ExprType::F64, IsAdd(true), def);
      case Expr::F64Sub:
        return EmitAddOrSub(f, ExprType::F64, IsAdd(false), def);
      case Expr::F64Mul:
        return EmitMultiply(f, ExprType::F64, def);
      case Expr::F64Div:
        return EmitDivOrMod(f, ExprType::F64, IsDiv(true), def);
      case Expr::F64Mod:
        return EmitDivOrMod(f, ExprType::F64, IsDiv(false), def);
      case Expr::F64Min:
        return EmitMathMinMax(f, ExprType::F64, IsMax(false), def);
      case Expr::F64Max:
        return EmitMathMinMax(f, ExprType::F64, IsMax(true), def);
      case Expr::F64Neg:
        return EmitUnaryMir<MAsmJSNeg>(f, ExprType::F64, def);
      case Expr::F64Abs:
        return EmitUnaryMir<MAbs>(f, ExprType::F64, def);
      case Expr::F64Sqrt:
        return EmitUnaryMir<MSqrt>(f, ExprType::F64, def);
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sin:
      case Expr::F64Cos:
      case Expr::F64Tan:
      case Expr::F64Asin:
      case Expr::F64Acos:
      case Expr::F64Atan:
      case Expr::F64Exp:
      case Expr::F64Log:
      case Expr::F64Pow:
      case Expr::F64Atan2:
        return EmitF64MathBuiltinCall(f, op, def);
      case Expr::F64FromF32:
        return EmitUnary<MToDouble>(f, ExprType::F32, def);
      case Expr::F64FromS32:
        return EmitUnary<MToDouble>(f, ExprType::I32, def);
      case Expr::F64FromU32:
        return EmitUnary<MAsmJSUnsignedToDouble>(f, ExprType::I32, def);
      case Expr::F64LoadMem:
        return EmitLoadArray(f, Scalar::Float64, def);
      case Expr::F64StoreMem:
        return EmitStore(f, Scalar::Float64, def);
      case Expr::F64StoreMemF32:
        return EmitStoreWithCoercion(f, Scalar::Float64, Scalar::Float32, def);
      // I32x4
      case Expr::I32X4Const:
        return EmitLiteral(f, ExprType::I32x4, def);
      case Expr::I32X4Ctor:
        return EmitSimdCtor(f, ExprType::I32x4, def);
      case Expr::I32X4Unary:
        return EmitSimdUnary(f, ExprType::I32x4, def);
      case Expr::I32X4Binary:
        return EmitSimdBinaryArith(f, ExprType::I32x4, def);
      case Expr::I32X4BinaryBitwise:
        return EmitSimdBinaryBitwise(f, ExprType::I32x4, def);
      case Expr::I32X4BinaryShift:
        return EmitSimdBinaryShift(f, def);
      case Expr::I32X4ReplaceLane:
        return EmitSimdReplaceLane(f, ExprType::I32x4, def);
      case Expr::I32X4FromF32X4:
        return EmitSimdCast<MSimdConvert>(f, ExprType::F32x4, ExprType::I32x4, def);
      case Expr::I32X4FromF32X4Bits:
        return EmitSimdCast<MSimdReinterpretCast>(f, ExprType::F32x4, ExprType::I32x4, def);
      case Expr::I32X4Swizzle:
        return EmitSimdSwizzle(f, ExprType::I32x4, def);
      case Expr::I32X4Shuffle:
        return EmitSimdShuffle(f, ExprType::I32x4, def);
      case Expr::I32X4Select:
        return EmitSimdSelect(f, ExprType::I32x4, def);
      case Expr::I32X4Splat:
        return EmitSimdSplat(f, ExprType::I32x4, def);
      case Expr::I32X4Load:
        return EmitSimdLoad(f, def);
      case Expr::I32X4Store:
        return EmitSimdStore(f, ExprType::I32x4, def);
      // F32x4
      case Expr::F32X4Const:
        return EmitLiteral(f, ExprType::F32x4, def);
      case Expr::F32X4Ctor:
        return EmitSimdCtor(f, ExprType::F32x4, def);
      case Expr::F32X4Unary:
        return EmitSimdUnary(f, ExprType::F32x4, def);
      case Expr::F32X4Binary:
        return EmitSimdBinaryArith(f, ExprType::F32x4, def);
      case Expr::F32X4ReplaceLane:
        return EmitSimdReplaceLane(f, ExprType::F32x4, def);
      case Expr::F32X4FromI32X4:
        return EmitSimdCast<MSimdConvert>(f, ExprType::I32x4, ExprType::F32x4, def);
      case Expr::F32X4FromI32X4Bits:
        return EmitSimdCast<MSimdReinterpretCast>(f, ExprType::I32x4, ExprType::F32x4, def);
      case Expr::F32X4Swizzle:
        return EmitSimdSwizzle(f, ExprType::F32x4, def);
      case Expr::F32X4Shuffle:
        return EmitSimdShuffle(f, ExprType::F32x4, def);
      case Expr::F32X4Select:
        return EmitSimdSelect(f, ExprType::F32x4, def);
      case Expr::F32X4Splat:
        return EmitSimdSplat(f, ExprType::F32x4, def);
      case Expr::F32X4Load:
        return EmitSimdLoad(f, def);
      case Expr::F32X4Store:
        return EmitSimdStore(f, ExprType::F32x4, def);
      // B32x4
      case Expr::B32X4Const:
        return EmitLiteral(f, ExprType::B32x4, def);
      case Expr::B32X4Ctor:
        return EmitSimdCtor(f, ExprType::B32x4, def);
      case Expr::B32X4Unary:
        return EmitSimdUnary(f, ExprType::B32x4, def);
      case Expr::B32X4Binary:
        return EmitSimdBinaryArith(f, ExprType::B32x4, def);
      case Expr::B32X4BinaryBitwise:
        return EmitSimdBinaryBitwise(f, ExprType::B32x4, def);
      case Expr::B32X4BinaryCompI32X4:
        return EmitSimdBinaryComp(f, ExprType::I32x4, def);
      case Expr::B32X4BinaryCompF32X4:
        return EmitSimdBinaryComp(f, ExprType::F32x4, def);
      case Expr::B32X4ReplaceLane:
        return EmitSimdReplaceLane(f, ExprType::B32x4, def);
      case Expr::B32X4Splat:
        return EmitSimdBooleanSplat(f, def);
      case Expr::Loop:
      case Expr::Select:
      case Expr::Br:
      case Expr::BrIf:
      case Expr::I8Const:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
      case Expr::F32CopySign:
      case Expr::F32Trunc:
      case Expr::F32NearestInt:
      case Expr::F64CopySign:
      case Expr::F64NearestInt:
      case Expr::F64Trunc:
      case Expr::I32UConvertF32:
      case Expr::I32UConvertF64:
      case Expr::I32ConvertI64:
      case Expr::I64Const:
      case Expr::I64SConvertI32:
      case Expr::I64UConvertI32:
      case Expr::I64SConvertF32:
      case Expr::I64SConvertF64:
      case Expr::I64UConvertF32:
      case Expr::I64UConvertF64:
      case Expr::I64LoadMem8S:
      case Expr::I64LoadMem16S:
      case Expr::I64LoadMem32S:
      case Expr::I64LoadMem8U:
      case Expr::I64LoadMem16U:
      case Expr::I64LoadMem32U:
      case Expr::I64LoadMem:
      case Expr::I64StoreMem8:
      case Expr::I64StoreMem16:
      case Expr::I64StoreMem32:
      case Expr::I64StoreMem:
        MOZ_CRASH("NYI");
      case Expr::DebugCheckPoint:
      case Expr::Unreachable:
        break;
    }

    MOZ_CRASH("unexpected wasm opcode");
}

static bool
EmitExprStmt(FunctionCompiler& f, MDefinition** def, LabelVector* maybeLabels)
{
    return EmitExpr(f, MaybeType::Undefined(), def, maybeLabels);
}

bool
wasm::IonCompileFunction(IonCompileTask* task)
{
    int64_t before = PRMJ_Now();

    const FuncBytecode& func = task->func();
    FuncCompileResults& results = task->results();

    JitContext jitContext(CompileRuntime::get(task->runtime()), &results.alloc());

    const JitCompileOptions options;
    MIRGraph graph(&results.alloc());
    CompileInfo compileInfo(func.numLocals());
    MIRGenerator mir(nullptr, options, &results.alloc(), &graph, &compileInfo,
                     IonOptimizations.get(OptimizationLevel::AsmJS),
                     task->args().useSignalHandlersForOOB);

    // Build MIR graph
    {
        FunctionCompiler f(task->mg(), func, mir, results);
        if (!f.init())
            return false;

        while (!f.done()) {
            MDefinition* _;
            if (!EmitExprStmt(f, &_))
                return false;
        }

        f.checkPostconditions();
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

        CodeGenerator codegen(&mir, lir, &results.masm());
        if (!codegen.generateAsmJS(&results.offsets()))
            return false;
    }

    results.setCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);
    return true;
}
