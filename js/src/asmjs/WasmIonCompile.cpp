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
#include "asmjs/WasmGenerator.h"

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
                ins = MConstant::NewInt64(alloc(), 0);
                break;
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
              case ValType::Limit:
                MOZ_CRASH("Limit");
            }

            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i), ins);
            if (!mirGen_.ensureBallast())
                return false;
        }

        addInterruptCheck();

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
        auto* ins = MSimdBinaryArith::New(alloc(), lhs, rhs, op);
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

    template<class T>
    MDefinition* binarySimd(MDefinition* lhs, MDefinition* rhs, typename T::Operation op)
    {
        if (inDeadCode())
            return nullptr;

        T* ins = T::New(alloc(), lhs, rhs, op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* swizzleSimd(MDefinition* vector, int32_t X, int32_t Y, int32_t Z, int32_t W,
                             MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(vector->type() == type);
        MSimdSwizzle* ins = MSimdSwizzle::New(alloc(), vector, X, Y, Z, W);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* shuffleSimd(MDefinition* lhs, MDefinition* rhs, int32_t X, int32_t Y,
                             int32_t Z, int32_t W, MIRType type)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(lhs->type() == type);
        MInstruction* ins = MSimdShuffle::New(alloc(), lhs, rhs, X, Y, Z, W);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* insertElementSimd(MDefinition* vec, MDefinition* val, SimdLane lane, MIRType type)
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

        MSimdAllTrue* ins = MSimdAllTrue::New(alloc(), boolVector, MIRType_Int32);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition* simdAnyTrue(MDefinition* boolVector)
    {
        if (inDeadCode())
            return nullptr;

        MSimdAnyTrue* ins = MSimdAnyTrue::New(alloc(), boolVector, MIRType_Int32);
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
        MSimdSplatX4* ins = MSimdSplatX4::New(alloc(), v, type);
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

    MDefinition* loadHeap(Scalar::Type accessType, MDefinition* ptr)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD loads should use loadSimdHeap");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr);
        curBlock_->add(load);
        return load;
    }

    MDefinition* loadSimdHeap(Scalar::Type accessType, MDefinition* ptr, unsigned numElems)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(Scalar::isSimdType(accessType), "loadSimdHeap can only load from a SIMD view");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr, numElems);
        curBlock_->add(load);
        return load;
    }

    void storeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MOZ_ASSERT(!Scalar::isSimdType(accessType), "SIMD stores should use storeSimdHeap");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v);
        curBlock_->add(store);
    }

    void storeSimdHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v,
                       unsigned numElems)
    {
        if (inDeadCode())
            return;

        MOZ_ASSERT(Scalar::isSimdType(accessType), "storeSimdHeap can only load from a SIMD view");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v, numElems);
        curBlock_->add(store);
    }

    void memoryBarrier(MemoryBarrierBits type)
    {
        if (inDeadCode())
            return;
        MMemoryBarrier* ins = MMemoryBarrier::New(alloc(), type);
        curBlock_->add(ins);
    }

    MDefinition* atomicLoadHeap(Scalar::Type accessType, MDefinition* ptr)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), accessType, ptr, /* numElems */ 0,
                                                   MembarBeforeLoad, MembarAfterLoad);
        curBlock_->add(load);
        return load;
    }

    void atomicStoreHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), accessType, ptr, v,
                                                      /* numElems = */ 0,
                                                      MembarBeforeStore, MembarAfterStore);
        curBlock_->add(store);
    }

    MDefinition* atomicCompareExchangeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* oldv,
                                           MDefinition* newv)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSCompareExchangeHeap* cas =
            MAsmJSCompareExchangeHeap::New(alloc(), accessType, ptr, oldv, newv);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicExchangeHeap(Scalar::Type accessType, MDefinition* ptr, MDefinition* value)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSAtomicExchangeHeap* cas =
            MAsmJSAtomicExchangeHeap::New(alloc(), accessType, ptr, value);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicBinopHeap(js::jit::AtomicOp op, Scalar::Type accessType, MDefinition* ptr,
                                 MDefinition* v)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSAtomicBinopHeap* binop =
            MAsmJSAtomicBinopHeap::New(alloc(), op, accessType, ptr, v);
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

    void addInterruptCheck()
    {
        if (mg_.args().useSignalHandlersForInterrupt)
            return;

        if (inDeadCode())
            return;

        // WasmHandleExecutionInterrupt takes 0 arguments and the stack is
        // always ABIStackAlignment-aligned, but don't forget to account for
        // ShadowStackSpace and any other ABI warts.
        ABIArgGenerator abi;
        if (abi.stackBytesConsumedSoFar() > mirGen_.maxAsmJSStackArgBytes())
            mirGen_.setAsmJSMaxStackArgBytes(abi.stackBytesConsumedSoFar());

        CallSiteDesc callDesc(0, CallSiteDesc::Relative);
        curBlock_->add(MAsmJSInterruptCheck::New(alloc()));
    }

    MDefinition* extractSimdElement(SimdLane lane, MDefinition* base, MIRType type, SimdSign sign)
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

    class Call
    {
        uint32_t lineOrBytecode_;
        ABIArgGenerator abi_;
        uint32_t prevMaxStackBytes_;
        uint32_t maxChildStackBytes_;
        uint32_t spIncrement_;
        MAsmJSCall::Args regArgs_;
        Vector<MAsmJSPassStackArg*, 0, SystemAllocPolicy> stackArgs_;
        bool childClobbers_;

        friend class FunctionCompiler;

      public:
        Call(FunctionCompiler& f, uint32_t lineOrBytecode)
          : lineOrBytecode_(lineOrBytecode),
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

        MAsmJSCall* ins = MAsmJSCall::New(alloc(), CallSiteDesc(call.lineOrBytecode_, kind),
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

    bool funcPtrCall(const Sig& sig, uint32_t length, uint32_t globalDataOffset, MDefinition* index,
                     const Call& call, MDefinition** def)
    {
        if (inDeadCode()) {
            *def = nullptr;
            return true;
        }

        MAsmJSLoadFuncPtr* ptrFun;
        if (mg().isAsmJS()) {
            MOZ_ASSERT(IsPowerOfTwo(length));
            MConstant* mask = MConstant::New(alloc(), Int32Value(length - 1));
            curBlock_->add(mask);
            MBitAnd* maskedIndex = MBitAnd::NewAsmJS(alloc(), index, mask, MIRType_Int32);
            curBlock_->add(maskedIndex);
            ptrFun = MAsmJSLoadFuncPtr::New(alloc(), maskedIndex, globalDataOffset);
            curBlock_->add(ptrFun);
        } else {
            // For wasm code, as a space optimization, the ModuleGenerator does not allocate a
            // table for signatures which do not contain any indirectly-callable functions.
            // However, these signatures may still be called (it is not a validation error)
            // so we instead have a flag alwaysThrow which throws an exception instead of loading
            // the function pointer from the (non-existant) array.
            MOZ_ASSERT(!length || length == mg_.numTableElems());
            bool alwaysThrow = !length;

            ptrFun = MAsmJSLoadFuncPtr::New(alloc(), index, mg_.numTableElems(), alwaysThrow,
                                            globalDataOffset);
            curBlock_->add(ptrFun);
        }

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
        MOZ_ASSERT(inDeadCode() || curBlock_ == block);
    }

    bool appendThenBlock(BlockVector* thenBlocks)
    {
        if (inDeadCode())
            return true;
        return thenBlocks->append(curBlock_);
    }

    bool joinIf(const BlockVector& thenBlocks, MBasicBlock* joinBlock)
    {
        MOZ_ASSERT_IF(curBlock_, thenBlocks.back() == curBlock_);
        curBlock_ = joinBlock;
        return joinIfElse(thenBlocks);
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

    bool usesPhiSlot()
    {
        return curBlock_->stackDepth() == info().firstStackSlot() + 1;
    }

    void pushPhiInput(MDefinition* def)
    {
        if (inDeadCode())
            return;
        MOZ_ASSERT(!usesPhiSlot());
        curBlock_->push(def);
    }

    MDefinition* popPhiOutput()
    {
        if (inDeadCode())
            return nullptr;
        MOZ_ASSERT(usesPhiSlot());
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
        if (cond->isConstant() && cond->toConstant()->valueToBooleanInfallible()) {
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
                if (cond->toConstant()->valueToBooleanInfallible()) {
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

    uint8_t        readU8()       { return decoder_.uncheckedReadFixedU8(); }
    uint32_t       readVarU32()   { return decoder_.uncheckedReadVarU32(); }
    uint64_t       readVarU64()   { return decoder_.uncheckedReadVarU64(); }
    float          readF32()      { return decoder_.uncheckedReadFixedF32(); }
    double         readF64()      { return decoder_.uncheckedReadFixedF64(); }
    Expr           readExpr()     { return decoder_.uncheckedReadExpr(); }
    Expr           peakExpr()     { return decoder_.uncheckedPeekExpr(); }

    SimdConstant readI32X4() {
        Val::I32x4 i32x4;
        JS_ALWAYS_TRUE(decoder_.readFixedI32x4(&i32x4));
        return SimdConstant::CreateX4(i32x4);
    }
    SimdConstant readF32X4() {
        Val::F32x4 f32x4;
        JS_ALWAYS_TRUE(decoder_.readFixedF32x4(&f32x4));
        return SimdConstant::CreateX4(f32x4);
    }

    uint32_t currentOffset() const {
        return decoder_.currentOffset();
    }
    uint32_t readCallSiteLineOrBytecode(uint32_t callOffset) {
        if (!func_.callSiteLineNums().empty())
            return func_.callSiteLineNums()[lastReadCallSite_++];
        return callOffset;
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

static bool
EmitLiteral(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    switch (type) {
      case ExprType::I32: {
        int32_t val = f.readVarU32();
        *def = f.constant(Int32Value(val), MIRType_Int32);
        return true;
      }
      case ExprType::I64: {
        int64_t val = f.readVarU64();
        *def = f.constant(val);
        return true;
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
      case ExprType::Void:
      case ExprType::Limit: {
        break;
      }
    }
    MOZ_CRASH("unexpected literal type");
}

static bool
EmitGetLocal(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    *def = f.getLocalDef(slot);
    MOZ_ASSERT_IF(type != ExprType::Void, f.localType(slot) == type);
    return true;
}

static bool
EmitLoadGlobal(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    *def = f.loadGlobalVar(global.globalDataOffset, global.isConst, ToMIRType(global.type));
    MOZ_ASSERT_IF(type != ExprType::Void, global.type == type);
    return true;
}

static bool EmitExpr(FunctionCompiler&, ExprType, MDefinition**, LabelVector* = nullptr);
static bool EmitExprStmt(FunctionCompiler&, MDefinition**, LabelVector* = nullptr);

static bool
EmitLoadStoreAddress(FunctionCompiler& f, Scalar::Type viewType, uint32_t* offset,
                     uint32_t* align, MDefinition** base)
{
    *offset = f.readVarU32();
    MOZ_ASSERT(*offset == 0, "Non-zero offsets not supported yet");

    *align = f.readVarU32();

    if (!EmitExpr(f, ExprType::I32, base))
        return false;

    // TODO Remove this (and the viewType param) after implementing unaligned
    // loads/stores.
    if (f.mg().isAsmJS())
        return true;

    int32_t maskVal = ~(Scalar::byteSize(viewType) - 1);
    if (maskVal == -1)
        return true;

    MDefinition* mask = f.constant(Int32Value(maskVal), MIRType_Int32);
    *base = f.bitwise<MBitAnd>(*base, mask, MIRType_Int32);
    return true;
}

static bool
EmitLoad(FunctionCompiler& f, Scalar::Type viewType, MDefinition** def)
{
    uint32_t offset, align;
    MDefinition* ptr;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &ptr))
        return false;
    *def = f.loadHeap(viewType, ptr);
    return true;
}

static bool
EmitStore(FunctionCompiler& f, Scalar::Type viewType, MDefinition** def)
{
    uint32_t offset, align;
    MDefinition* ptr;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &ptr))
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

    f.storeHeap(viewType, ptr, rhs);
    *def = rhs;
    return true;
}

static bool
EmitStoreWithCoercion(FunctionCompiler& f, Scalar::Type rhsType, Scalar::Type viewType,
                      MDefinition **def)
{
    uint32_t offset, align;
    MDefinition* ptr;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &ptr))
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

    f.storeHeap(viewType, ptr, coerced);
    *def = rhs;
    return true;
}

static bool
EmitSetLocal(FunctionCompiler& f, ExprType expected, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    MDefinition* expr;
    ExprType actual = f.localType(slot);
    MOZ_ASSERT_IF(expected != ExprType::Void, actual == expected);
    if (!EmitExpr(f, actual, &expr))
        return false;
    f.assign(slot, expr);
    *def = expr;
    return true;
}

static bool
EmitStoreGlobal(FunctionCompiler& f, ExprType type, MDefinition**def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    MOZ_ASSERT_IF(type != ExprType::Void, global.type == type);
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
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.minMax(lhs, rhs, mirType, isMax);
    return true;
}

static bool
EmitAtomicsLoad(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    uint32_t offset, align;
    MDefinition* index;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &index))
        return false;

    *def = f.atomicLoadHeap(viewType, index);
    return true;
}

static bool
EmitAtomicsStore(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    uint32_t offset, align;
    MDefinition* index;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &index))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    f.atomicStoreHeap(viewType, index, value);
    *def = value;
    return true;
}

static bool
EmitAtomicsBinOp(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());
    js::jit::AtomicOp op = js::jit::AtomicOp(f.readU8());

    uint32_t offset, align;
    MDefinition* index;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &index))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    *def = f.atomicBinopHeap(op, viewType, index, value);
    return true;
}

static bool
EmitAtomicsCompareExchange(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    uint32_t offset, align;
    MDefinition* index;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &index))
        return false;

    MDefinition* oldValue;
    if (!EmitExpr(f, ExprType::I32, &oldValue))
        return false;
    MDefinition* newValue;
    if (!EmitExpr(f, ExprType::I32, &newValue))
        return false;
    *def = f.atomicCompareExchangeHeap(viewType, index, oldValue, newValue);
    return true;
}

static bool
EmitAtomicsExchange(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    uint32_t offset, align;
    MDefinition* index;
    if (!EmitLoadStoreAddress(f, viewType, &offset, &align, &index))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, ExprType::I32, &value))
        return false;
    *def = f.atomicExchangeHeap(viewType, index, value);
    return true;
}

static bool
EmitCallArgs(FunctionCompiler& f, const Sig& sig, FunctionCompiler::Call* call)
{
    f.startCallArgs(call);
    for (ValType argType : sig.args()) {
        MDefinition* arg;
        if (!EmitExpr(f, ToExprType(argType), &arg))
            return false;
        if (!f.passArg(arg, argType, call))
            return false;
    }
    f.finishCallArgs(call);
    return true;
}

static bool
EmitCall(FunctionCompiler& f, uint32_t callOffset, ExprType ret, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t funcIndex = f.readVarU32();

    const Sig& sig = f.mg().funcSig(funcIndex);
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret != ExprType::Void, sig.ret() == ret);

    FunctionCompiler::Call call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.internalCall(sig, funcIndex, call, def);
}

static bool
EmitCallIndirect(FunctionCompiler& f, uint32_t callOffset, ExprType ret, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t sigIndex = f.readVarU32();

    const Sig& sig = f.mg().sig(sigIndex);
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret != ExprType::Void, sig.ret() == ret);

    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    FunctionCompiler::Call call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    const TableModuleGeneratorData& table = f.mg().sigToTable(sigIndex);
    return f.funcPtrCall(sig, table.numElems, table.globalDataOffset, index, call, def);
}

static bool
EmitCallImport(FunctionCompiler& f, uint32_t callOffset, ExprType ret, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t importIndex = f.readVarU32();

    const ImportModuleGeneratorData& import = f.mg().import(importIndex);
    const Sig& sig = *import.sig;
    MOZ_ASSERT_IF(!IsVoid(sig.ret()) && ret != ExprType::Void, sig.ret() == ret);

    FunctionCompiler::Call call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.ffiCall(import.globalDataOffset, call, sig.ret(), def);
}

static bool
EmitF32MathBuiltinCall(FunctionCompiler& f, uint32_t callOffset, Expr f32, MDefinition** def)
{
    MOZ_ASSERT(f32 == Expr::F32Ceil || f32 == Expr::F32Floor);

    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    FunctionCompiler::Call call(f, lineOrBytecode);
    f.startCallArgs(&call);

    MDefinition* firstArg;
    if (!EmitExpr(f, ExprType::F32, &firstArg) || !f.passArg(firstArg, ValType::F32, &call))
        return false;

    f.finishCallArgs(&call);

    SymbolicAddress callee = f32 == Expr::F32Ceil ? SymbolicAddress::CeilF : SymbolicAddress::FloorF;
    return f.builtinCall(callee, call, ValType::F32, def);
}

static bool
EmitF64MathBuiltinCall(FunctionCompiler& f, uint32_t callOffset, Expr f64, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);

    FunctionCompiler::Call call(f, lineOrBytecode);
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
EmitSimdUnary(FunctionCompiler& f, ExprType type, SimdOperation simdOp, MDefinition** def)
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
    MDefinition* in;
    if (!EmitExpr(f, type, &in))
        return false;
    *def = f.unarySimd(in, op, ToMIRType(type));
    return true;
}

template<class OpKind>
inline bool
EmitSimdBinary(FunctionCompiler& f, ExprType type, OpKind op, MDefinition** def)
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
EmitSimdBinaryComp(FunctionCompiler& f, ExprType type, MSimdBinaryComp::Operation op,
                   SimdSign sign, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    *def = f.binarySimdComp(lhs, rhs, op, sign);
    return true;
}

static bool
EmitSimdShift(FunctionCompiler& f, ExprType type, MSimdShift::Operation op, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
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
      case ExprType::Void:
      case ExprType::Limit:;
    }
    MOZ_CRASH("bad simd type");
}

static bool
EmitExtractLane(FunctionCompiler& f, ExprType type, SimdSign sign, MDefinition** def)
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
    int32_t laneLit = laneDef->toConstant()->toInt32();
    MOZ_ASSERT(laneLit < 4);
    SimdLane lane = SimdLane(laneLit);

    *def = f.extractSimdElement(lane, vec, ToMIRType(SimdToLaneType(type)), sign);
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
        int32_t laneLit = laneDef->toConstant()->toInt32();
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

inline bool
EmitSimdBitcast(FunctionCompiler& f, ExprType fromType, ExprType toType, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, fromType, &in))
        return false;
    *def = f.bitcastSimd(in, ToMIRType(fromType), ToMIRType(toType));
    return true;
}

inline bool
EmitSimdConvert(FunctionCompiler& f, ExprType fromType, ExprType toType, SimdSign sign,
                MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, fromType, &in))
        return false;
    *def = f.convertSimd(in, ToMIRType(fromType), ToMIRType(toType), sign);
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

static inline Scalar::Type
SimdExprTypeToViewType(ExprType type, unsigned* defaultNumElems)
{
    switch (type) {
        case ExprType::I32x4: *defaultNumElems = 4; return Scalar::Int32x4;
        case ExprType::F32x4: *defaultNumElems = 4; return Scalar::Float32x4;
        default:              break;
    }
    MOZ_CRASH("type not handled in SimdExprTypeToViewType");
}

static bool
EmitSimdLoad(FunctionCompiler& f, ExprType type, unsigned numElems, MDefinition** def)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(type, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    *def = f.loadSimdHeap(viewType, index, numElems);
    return true;
}

static bool
EmitSimdStore(FunctionCompiler& f, ExprType type, unsigned numElems, MDefinition** def)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(type, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    MDefinition* index;
    if (!EmitExpr(f, ExprType::I32, &index))
        return false;

    MDefinition* vec;
    if (!EmitExpr(f, type, &vec))
        return false;

    f.storeSimdHeap(viewType, index, vec, numElems);
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
    if (IsSimdBoolType(type)) {
        if (!EmitSimdBooleanLaneExpr(f, &in))
            return false;
    } else if (!EmitExpr(f, SimdToLaneType(type), &in)) {
        return false;
    }
    *def = f.splatSimd(in, ToMIRType(type));
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
      case ExprType::Limit:
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
    MOZ_ASSERT(type != ExprType::I32 && type != ExprType::I64,
               "int div or mod must indicate signedness");
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
      case Expr::I64Eq:
      case Expr::I64Ne:
      case Expr::I64LeS:
      case Expr::I64LtS:
      case Expr::I64LeU:
      case Expr::I64LtU:
      case Expr::I64GeS:
      case Expr::I64GtS:
      case Expr::I64GeU:
      case Expr::I64GtU:
        if (!EmitExpr(f, ExprType::I64, &lhs) || !EmitExpr(f, ExprType::I64, &rhs))
            return false;
        switch (expr) {
          case Expr::I64LeS: case Expr::I64LtS: case Expr::I64GeS: case Expr::I64GtS:
          case Expr::I64Eq: case Expr::I64Ne:
            compareType = MCompare::Compare_Int64;
            break;
          case Expr::I64GeU: case Expr::I64GtU: case Expr::I64LeU: case Expr::I64LtU:
            compareType = MCompare::Compare_UInt64;
            break;
          default:
            MOZ_CRASH("unexpected opcode");
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
      case Expr::I64Eq:
      case Expr::F32Eq:
      case Expr::F64Eq:
        compareOp = JSOP_EQ;
        break;
      case Expr::I32Ne:
      case Expr::I64Ne:
      case Expr::F32Ne:
      case Expr::F64Ne:
        compareOp = JSOP_NE;
        break;
      case Expr::I32LeS:
      case Expr::I32LeU:
      case Expr::I64LeS:
      case Expr::I64LeU:
      case Expr::F32Le:
      case Expr::F64Le:
        compareOp = JSOP_LE;
        break;
      case Expr::I32LtS:
      case Expr::I32LtU:
      case Expr::I64LtS:
      case Expr::I64LtU:
      case Expr::F32Lt:
      case Expr::F64Lt:
        compareOp = JSOP_LT;
        break;
      case Expr::I32GeS:
      case Expr::I32GeU:
      case Expr::I64GeS:
      case Expr::I64GeU:
      case Expr::F32Ge:
      case Expr::F64Ge:
        compareOp = JSOP_GE;
        break;
      case Expr::I32GtS:
      case Expr::I32GtU:
      case Expr::I64GtS:
      case Expr::I64GtU:
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
EmitBitwise(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, type, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, type, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.bitwise<T>(lhs, rhs, mirType);
    return true;
}

static bool
EmitBitwiseNot(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, ExprType::I32, &in))
        return false;
    *def = f.bitwise<MBitNot>(in);
    return true;
}

static bool
EmitSimdOp(FunctionCompiler& f, ExprType type, SimdOperation op, SimdSign sign, MDefinition** def)
{
    switch (op) {
      case SimdOperation::Constructor:
        return EmitSimdCtor(f, type, def);
      case SimdOperation::Fn_extractLane:
        return EmitExtractLane(f, type, sign, def);
      case SimdOperation::Fn_replaceLane:
        return EmitSimdReplaceLane(f, type, def);
      case SimdOperation::Fn_check:
        MOZ_CRASH("only used in asm.js' type system");
      case SimdOperation::Fn_splat:
        return EmitSimdSplat(f, type, def);
      case SimdOperation::Fn_select:
        return EmitSimdSelect(f, type, def);
      case SimdOperation::Fn_swizzle:
        return EmitSimdSwizzle(f, type, def);
      case SimdOperation::Fn_shuffle:
        return EmitSimdShuffle(f, type, def);
      case SimdOperation::Fn_load:
        return EmitSimdLoad(f, type, 0, def);
      case SimdOperation::Fn_load1:
        return EmitSimdLoad(f, type, 1, def);
      case SimdOperation::Fn_load2:
        return EmitSimdLoad(f, type, 2, def);
      case SimdOperation::Fn_load3:
        return EmitSimdLoad(f, type, 3, def);
      case SimdOperation::Fn_store:
        return EmitSimdStore(f, type, 0, def);
      case SimdOperation::Fn_store1:
        return EmitSimdStore(f, type, 1, def);
      case SimdOperation::Fn_store2:
        return EmitSimdStore(f, type, 2, def);
      case SimdOperation::Fn_store3:
        return EmitSimdStore(f, type, 3, def);
      case SimdOperation::Fn_allTrue:
        return EmitSimdAllTrue(f, type, def);
      case SimdOperation::Fn_anyTrue:
        return EmitSimdAnyTrue(f, type, def);
      case SimdOperation::Fn_abs:
      case SimdOperation::Fn_neg:
      case SimdOperation::Fn_not:
      case SimdOperation::Fn_sqrt:
      case SimdOperation::Fn_reciprocalApproximation:
      case SimdOperation::Fn_reciprocalSqrtApproximation:
        return EmitSimdUnary(f, type, op, def);
      case SimdOperation::Fn_shiftLeftByScalar:
        return EmitSimdShift(f, type, MSimdShift::lsh, def);
      case SimdOperation::Fn_shiftRightByScalar:
        return EmitSimdShift(f, type, MSimdShift::rshForSign(sign), def);
#define _CASE(OP) \
      case SimdOperation::Fn_##OP: \
        return EmitSimdBinaryComp(f, type, MSimdBinaryComp::OP, sign, def);
        FOREACH_COMP_SIMD_OP(_CASE)
#undef _CASE
      case SimdOperation::Fn_and:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::and_, def);
      case SimdOperation::Fn_or:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::or_, def);
      case SimdOperation::Fn_xor:
        return EmitSimdBinary(f, type, MSimdBinaryBitwise::xor_, def);
#define _CASE(OP) \
      case SimdOperation::Fn_##OP: \
        return EmitSimdBinary(f, type, MSimdBinaryArith::Op_##OP, def);
      FOREACH_NUMERIC_SIMD_BINOP(_CASE)
      FOREACH_FLOAT_SIMD_BINOP(_CASE)
#undef _CASE
      case SimdOperation::Fn_fromFloat32x4:
        return EmitSimdConvert(f, ExprType::F32x4, type, sign, def);
      case SimdOperation::Fn_fromInt32x4:
        return EmitSimdConvert(f, ExprType::I32x4, type, SimdSign::Signed, def);
      case SimdOperation::Fn_fromUint32x4:
        return EmitSimdConvert(f, ExprType::I32x4, type, SimdSign::Unsigned, def);
      case SimdOperation::Fn_fromInt32x4Bits:
      case SimdOperation::Fn_fromUint32x4Bits:
        return EmitSimdBitcast(f, ExprType::I32x4, type, def);
      case SimdOperation::Fn_fromFloat32x4Bits:
        return EmitSimdBitcast(f, ExprType::F32x4, type, def);
      case SimdOperation::Fn_fromInt8x16Bits:
      case SimdOperation::Fn_fromInt16x8Bits:
      case SimdOperation::Fn_fromUint8x16Bits:
      case SimdOperation::Fn_fromUint16x8Bits:
      case SimdOperation::Fn_fromFloat64x2Bits:
        MOZ_CRASH("NYI");
    }
    MOZ_CRASH("unexpected opcode");
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

    f.addInterruptCheck();

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

    f.addInterruptCheck();

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

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
EmitDoWhile(FunctionCompiler& f, const LabelVector* maybeLabels)
{
    size_t headId = f.nextId();

    MBasicBlock* loopEntry;
    if (!f.startPendingLoop(headId, &loopEntry))
        return false;

    f.addInterruptCheck();

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
    uint32_t labelId = f.readVarU32();

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
EmitIfElse(FunctionCompiler& f, bool hasElse, ExprType expected, MDefinition** def)
{
    // Handle if/else-if chains using iteration instead of recursion. This
    // avoids blowing the C stack quota for long if/else-if chains and also
    // creates fewer MBasicBlocks at join points (by creating one join block
    // for the entire if/else-if chain).
    BlockVector thenBlocks;

    *def = nullptr;

  recurse:
    MOZ_ASSERT_IF(!hasElse, expected == ExprType::Void);

    MDefinition* condition;
    if (!EmitExpr(f, ExprType::I32, &condition))
        return false;

    MBasicBlock* thenBlock = nullptr;
    MBasicBlock* elseOrJoinBlock = nullptr;
    if (!f.branchAndStartThen(condition, &thenBlock, &elseOrJoinBlock))
        return false;

    // From this point, then block.
    MDefinition* ifExpr;
    if (!EmitExpr(f, expected, &ifExpr))
        return false;

    if (expected != ExprType::Void)
        f.pushPhiInput(ifExpr);

    if (!f.appendThenBlock(&thenBlocks))
        return false;

    if (!hasElse)
        return f.joinIf(thenBlocks, elseOrJoinBlock);

    f.switchToElse(elseOrJoinBlock);

    Expr nextStmt = f.peakExpr();
    if (nextStmt == Expr::If || nextStmt == Expr::IfElse) {
        hasElse = nextStmt == Expr::IfElse;
        JS_ALWAYS_TRUE(f.readExpr() == nextStmt);
        goto recurse;
    }

    // From this point, else block.
    MDefinition* elseExpr;
    if (!EmitExpr(f, expected, &elseExpr))
        return false;

    if (expected != ExprType::Void)
        f.pushPhiInput(elseExpr);

    if (!f.joinIfElse(thenBlocks))
        return false;

    // Now we're on the join block.
    if (expected != ExprType::Void)
        *def = f.popPhiOutput();

    MOZ_ASSERT_IF(!f.inDeadCode(), (expected != ExprType::Void) == !!*def);
    return true;
}

static bool
EmitTableSwitch(FunctionCompiler& f)
{
    bool hasDefault = f.readU8();
    int32_t low = f.readVarU32();
    int32_t high = f.readVarU32();
    uint32_t numCases = f.readVarU32();

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
        int32_t caseValue = f.readVarU32();
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
EmitReturn(FunctionCompiler& f)
{
    ExprType ret = f.sig().ret();

    if (IsVoid(ret)) {
        f.returnVoid();
        return true;
    }

    MDefinition* def;
    if (!EmitExpr(f, ret, &def))
        return false;
    f.returnExpr(def);
    return true;
}

static bool
EmitBlock(FunctionCompiler& f, ExprType type, MDefinition** def)
{
    uint32_t numStmts = f.readVarU32();
    if (numStmts) {
        for (uint32_t i = 0; i < numStmts - 1; i++) {
            // Fine to clobber def, we only want the last use.
            if (!EmitExprStmt(f, def))
                return false;
        }
        if (!EmitExpr(f, type, def))
            return false;
    }
    return true;
}

typedef bool HasLabel;

static bool
EmitContinue(FunctionCompiler& f, bool hasLabel)
{
    if (!hasLabel)
        return f.addContinue(nullptr);
    uint32_t labelId = f.readVarU32();
    return f.addContinue(&labelId);
}

static bool
EmitBreak(FunctionCompiler& f, bool hasLabel)
{
    if (!hasLabel)
        return f.addBreak(nullptr);
    uint32_t labelId = f.readVarU32();
    return f.addBreak(&labelId);
}

static bool
EmitExpr(FunctionCompiler& f, ExprType type, MDefinition** def, LabelVector* maybeLabels)
{
    if (!f.mirGen().ensureBallast())
        return false;

    uint32_t exprOffset = f.currentOffset();

    switch (Expr op = f.readExpr()) {
      case Expr::Nop:
        *def = nullptr;
        return true;
      case Expr::Block:
        return EmitBlock(f, type, def);
      case Expr::If:
      case Expr::IfElse:
        return EmitIfElse(f, HasElseBlock(op == Expr::IfElse), type, def);
      case Expr::TableSwitch:
        return EmitTableSwitch(f);
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
        return EmitReturn(f);
      case Expr::Call:
        return EmitCall(f, exprOffset, type, def);
      case Expr::CallIndirect:
        return EmitCallIndirect(f, exprOffset, type, def);
      case Expr::CallImport:
        return EmitCallImport(f, exprOffset, type, def);
      case Expr::AtomicsFence:
        f.memoryBarrier(MembarFull);
        return true;
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
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
        return EmitUnary<MTruncateToInt32>(f, ExprType::F32, def);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return EmitUnary<MTruncateToInt32>(f, ExprType::F64, def);
      case Expr::I32Clz:
        return EmitUnary<MClz>(f, ExprType::I32, def);
      case Expr::I32Popcnt:
        return EmitUnary<MPopcnt>(f, ExprType::I32, def);
      case Expr::I32Abs:
        return EmitUnaryMir<MAbs>(f, ExprType::I32, def);
      case Expr::I32Neg:
        return EmitUnaryMir<MAsmJSNeg>(f, ExprType::I32, def);
      case Expr::I32Or:
        return EmitBitwise<MBitOr>(f, ExprType::I32, def);
      case Expr::I32And:
        return EmitBitwise<MBitAnd>(f, ExprType::I32, def);
      case Expr::I32Xor:
        return EmitBitwise<MBitXor>(f, ExprType::I32, def);
      case Expr::I32Shl:
        return EmitBitwise<MLsh>(f, ExprType::I32, def);
      case Expr::I32ShrS:
        return EmitBitwise<MRsh>(f, ExprType::I32, def);
      case Expr::I32ShrU:
        return EmitBitwise<MUrsh>(f, ExprType::I32, def);
      case Expr::I32BitNot:
        return EmitBitwiseNot(f, def);
      case Expr::I32LoadMem8S:
        return EmitLoad(f, Scalar::Int8, def);
      case Expr::I32LoadMem8U:
        return EmitLoad(f, Scalar::Uint8, def);
      case Expr::I32LoadMem16S:
        return EmitLoad(f, Scalar::Int16, def);
      case Expr::I32LoadMem16U:
        return EmitLoad(f, Scalar::Uint16, def);
      case Expr::I32LoadMem:
        return EmitLoad(f, Scalar::Int32, def);
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
      case Expr::I64Eq:
      case Expr::I64Ne:
      case Expr::I64LtS:
      case Expr::I64LeS:
      case Expr::I64LtU:
      case Expr::I64LeU:
      case Expr::I64GtS:
      case Expr::I64GeS:
      case Expr::I64GtU:
      case Expr::I64GeU:
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
      // I64
      case Expr::I64Const:
        return EmitLiteral(f, ExprType::I64, def);
      case Expr::I64Or:
        return EmitBitwise<MBitOr>(f, ExprType::I64, def);
      case Expr::I64And:
        return EmitBitwise<MBitAnd>(f, ExprType::I64, def);
      case Expr::I64Xor:
        return EmitBitwise<MBitXor>(f, ExprType::I64, def);
      case Expr::I64Shl:
        return EmitBitwise<MLsh>(f, ExprType::I64, def);
      case Expr::I64ShrS:
        return EmitBitwise<MRsh>(f, ExprType::I64, def);
      case Expr::I64ShrU:
        return EmitBitwise<MUrsh>(f, ExprType::I64, def);
      case Expr::I64Add:
        return EmitAddOrSub(f, ExprType::I64, IsAdd(true), def);
      case Expr::I64Sub:
        return EmitAddOrSub(f, ExprType::I64, IsAdd(false), def);
      case Expr::I64Mul:
        return EmitMultiply(f, ExprType::I64, def);
      case Expr::I64DivS:
      case Expr::I64DivU:
        return EmitDivOrMod(f, ExprType::I64, IsDiv(true), IsUnsigned(op == Expr::I64DivU), def);
      case Expr::I64RemS:
      case Expr::I64RemU:
        return EmitDivOrMod(f, ExprType::I64, IsDiv(false), IsUnsigned(op == Expr::I64RemU), def);
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
        return EmitF32MathBuiltinCall(f, exprOffset, op, def);
      case Expr::F32DemoteF64:
        return EmitUnary<MToFloat32>(f, ExprType::F64, def);
      case Expr::F32ConvertSI32:
        return EmitUnary<MToFloat32>(f, ExprType::I32, def);
      case Expr::F32ConvertUI32:
        return EmitUnary<MAsmJSUnsignedToFloat32>(f, ExprType::I32, def);
      case Expr::F32LoadMem:
        return EmitLoad(f, Scalar::Float32, def);
      case Expr::F32StoreMem:
        return EmitStore(f, Scalar::Float32, def);
      case Expr::F32StoreMemF64:
        return EmitStoreWithCoercion(f, Scalar::Float32, Scalar::Float64, def);
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
        return EmitF64MathBuiltinCall(f, exprOffset, op, def);
      case Expr::F64PromoteF32:
        return EmitUnary<MToDouble>(f, ExprType::F32, def);
      case Expr::F64ConvertSI32:
        return EmitUnary<MToDouble>(f, ExprType::I32, def);
      case Expr::F64ConvertUI32:
        return EmitUnary<MAsmJSUnsignedToDouble>(f, ExprType::I32, def);
      case Expr::F64LoadMem:
        return EmitLoad(f, Scalar::Float64, def);
      case Expr::F64StoreMem:
        return EmitStore(f, Scalar::Float64, def);
      case Expr::F64StoreMemF32:
        return EmitStoreWithCoercion(f, Scalar::Float64, Scalar::Float32, def);
      // SIMD
#define CASE(TYPE, OP, SIGN)                                               \
      case Expr::TYPE##OP:                                                 \
        return EmitSimdOp(f, ExprType::TYPE, SimdOperation::Fn_##OP, SIGN, def);
#define I32CASE(OP) CASE(I32x4, OP, SimdSign::Signed)
#define F32CASE(OP) CASE(F32x4, OP, SimdSign::NotApplicable)
#define B32CASE(OP) CASE(B32x4, OP, SimdSign::NotApplicable)
#define ENUMERATE(TYPE, FORALL, DO)                                            \
      case Expr::TYPE##Const:                                                  \
        return EmitLiteral(f, ExprType::TYPE, def);                            \
      case Expr::TYPE##Constructor:                                            \
        return EmitSimdOp(f, ExprType::TYPE, SimdOperation::Constructor,       \
                          SimdSign::NotApplicable, def);                       \
      FORALL(DO)

      ENUMERATE(I32x4, FORALL_INT32X4_ASMJS_OP, I32CASE)
      ENUMERATE(F32x4, FORALL_FLOAT32X4_ASMJS_OP, F32CASE)
      ENUMERATE(B32x4, FORALL_BOOL_SIMD_OP, B32CASE)

#undef CASE
#undef I32CASE
#undef F32CASE
#undef B32CASE
#undef ENUMERATE
      // SIMD unsigned integer operations.
      case Expr::I32x4shiftRightByScalarU:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_shiftRightByScalar,
                          SimdSign::Unsigned, def);
      case Expr::I32x4lessThanU:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_lessThan, SimdSign::Unsigned, def);
      case Expr::I32x4lessThanOrEqualU:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_lessThanOrEqual,
                          SimdSign::Unsigned, def);
      case Expr::I32x4greaterThanU:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_greaterThan, SimdSign::Unsigned,
                          def);
      case Expr::I32x4greaterThanOrEqualU:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_greaterThanOrEqual,
                          SimdSign::Unsigned, def);
      case Expr::I32x4fromFloat32x4U:
        return EmitSimdOp(f, ExprType::I32x4, SimdOperation::Fn_fromFloat32x4,
                          SimdSign::Unsigned, def);

      // Future opcodes
      case Expr::Loop:
      case Expr::Select:
      case Expr::Br:
      case Expr::BrIf:
      case Expr::I32Ctz:
      case Expr::F32CopySign:
      case Expr::F32Trunc:
      case Expr::F32Nearest:
      case Expr::F64CopySign:
      case Expr::F64Nearest:
      case Expr::F64Trunc:
      case Expr::I32WrapI64:
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
      case Expr::I64TruncSF32:
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF32:
      case Expr::I64TruncUF64:
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::I64ReinterpretF64:
      case Expr::F64ReinterpretI64:
      case Expr::I32ReinterpretF32:
      case Expr::F32ReinterpretI32:
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
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        MOZ_CRASH("NYI");
      case Expr::Unreachable:
        break;
      case Expr::Limit:
        MOZ_CRASH("Limit");
    }

    MOZ_CRASH("unexpected wasm opcode");
}

static bool
EmitExprStmt(FunctionCompiler& f, MDefinition** def, LabelVector* maybeLabels)
{
    return EmitExpr(f, ExprType::Void, def, maybeLabels);
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
                     IonOptimizations.get(OptimizationLevel::AsmJS));
    mir.initUsesSignalHandlersForAsmJSOOB(task->mg().args().useSignalHandlersForOOB);
    mir.initMinAsmJSHeapLength(task->mg().minHeapLength());

    // Build MIR graph
    {
        FunctionCompiler f(task->mg(), func, mir, results);
        if (!f.init())
            return false;

        MDefinition* last = nullptr;
        if (uint32_t numExprs = f.readVarU32()) {
            for (uint32_t i = 0; i < numExprs - 1; i++) {
                if (!EmitExpr(f, ExprType::Void, &last))
                    return false;
            }

            if (!EmitExpr(f, f.sig().ret(), &last))
                return false;
        }

        MOZ_ASSERT(f.done());
        MOZ_ASSERT(IsVoid(f.sig().ret()) || f.inDeadCode() || last);

        if (IsVoid(f.sig().ret()))
            f.returnVoid();
        else
            f.returnExpr(last);

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
