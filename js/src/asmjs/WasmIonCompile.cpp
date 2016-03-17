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

typedef Vector<MBasicBlock*, 8, SystemAllocPolicy> BlockVector;

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph.
class FunctionCompiler
{
  private:
    typedef Vector<BlockVector, 0, SystemAllocPolicy> BlocksVector;

    ModuleGeneratorThreadView& mg_;
    Decoder&                   decoder_;
    const FuncBytes&           func_;
    const ValTypeVector&       locals_;
    size_t                     lastReadCallSite_;

    TempAllocator&             alloc_;
    MIRGraph&                  graph_;
    const CompileInfo&         info_;
    MIRGenerator&              mirGen_;

    MBasicBlock*               curBlock_;

    uint32_t                   loopDepth_;
    uint32_t                   blockDepth_;
    BlocksVector               targets_;

    FuncCompileResults&        compileResults_;

  public:
    FunctionCompiler(ModuleGeneratorThreadView& mg,
                     Decoder& decoder,
                     const FuncBytes& func,
                     const ValTypeVector& locals,
                     MIRGenerator& mirGen,
                     FuncCompileResults& compileResults)
      : mg_(mg),
        decoder_(decoder),
        func_(func),
        locals_(locals),
        lastReadCallSite_(0),
        alloc_(mirGen.alloc()),
        graph_(mirGen.graph()),
        info_(mirGen.info()),
        mirGen_(mirGen),
        curBlock_(nullptr),
        loopDepth_(0),
        blockDepth_(0),
        compileResults_(compileResults)
    {}

    ModuleGeneratorThreadView& mg() const    { return mg_; }
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

        for (size_t i = args.length(); i < locals_.length(); i++) {
            MInstruction* ins = nullptr;
            switch (locals_[i]) {
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
        MOZ_ASSERT(loopDepth_ == 0);
        MOZ_ASSERT(blockDepth_ == 0);
#ifdef DEBUG
        for (BlockVector& vec : targets_) {
            MOZ_ASSERT(vec.empty());
        }
#endif
        MOZ_ASSERT(inDeadCode());
        MOZ_ASSERT(decoder_.done(), "all bytes must be consumed");
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

    ValType localType(unsigned slot) const { return locals_[slot]; }

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

    MDefinition* loadHeap(MDefinition* base,
                          const MAsmJSHeapAccess& access)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(!Scalar::isSimdType(access.accessType()), "SIMD loads should use loadSimdHeap");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), base, access);
        curBlock_->add(load);
        return load;
    }

    MDefinition* loadSimdHeap(MDefinition* base, const MAsmJSHeapAccess& access)
    {
        if (inDeadCode())
            return nullptr;

        MOZ_ASSERT(Scalar::isSimdType(access.accessType()),
                   "loadSimdHeap can only load from a SIMD view");
        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), base, access);
        curBlock_->add(load);
        return load;
    }

    void storeHeap(MDefinition* base, const MAsmJSHeapAccess& access, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MOZ_ASSERT(!Scalar::isSimdType(access.accessType()),
                   "SIMD stores should use storeSimdHeap");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), base, access, v);
        curBlock_->add(store);
    }

    void storeSimdHeap(MDefinition* base, const MAsmJSHeapAccess& access, MDefinition* v)
    {
        if (inDeadCode())
            return;

        MOZ_ASSERT(Scalar::isSimdType(access.accessType()),
                   "storeSimdHeap can only load from a SIMD view");
        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), base, access, v);
        curBlock_->add(store);
    }

    void memoryBarrier(MemoryBarrierBits type)
    {
        if (inDeadCode())
            return;
        MMemoryBarrier* ins = MMemoryBarrier::New(alloc(), type);
        curBlock_->add(ins);
    }

    MDefinition* atomicLoadHeap(MDefinition* base, const MAsmJSHeapAccess& access)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSLoadHeap* load = MAsmJSLoadHeap::New(alloc(), base, access);
        curBlock_->add(load);
        return load;
    }

    void atomicStoreHeap(MDefinition* base, const MAsmJSHeapAccess& access,
                         MDefinition* v)
    {
        if (inDeadCode())
            return;

        MAsmJSStoreHeap* store = MAsmJSStoreHeap::New(alloc(), base, access, v);
        curBlock_->add(store);
    }

    MDefinition* atomicCompareExchangeHeap(MDefinition* base, const MAsmJSHeapAccess& access,
                                           MDefinition* oldv, MDefinition* newv)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSCompareExchangeHeap* cas =
            MAsmJSCompareExchangeHeap::New(alloc(), base, access, oldv, newv);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicExchangeHeap(MDefinition* base, const MAsmJSHeapAccess& access,
                                    MDefinition* value)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSAtomicExchangeHeap* cas =
            MAsmJSAtomicExchangeHeap::New(alloc(), base, access, value);
        curBlock_->add(cas);
        return cas;
    }

    MDefinition* atomicBinopHeap(js::jit::AtomicOp op,
                                 MDefinition* base, const MAsmJSHeapAccess& access,
                                 MDefinition* v)
    {
        if (inDeadCode())
            return nullptr;

        MAsmJSAtomicBinopHeap* binop =
            MAsmJSAtomicBinopHeap::New(alloc(), op, base, access, v);
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

    bool unreachableTrap()
    {
        if (inDeadCode())
            return true;

        auto* ins = MAsmThrowUnreachable::New(alloc());
        curBlock_->end(ins);
        curBlock_ = nullptr;
        return true;
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

  private:
    static bool hasPushed(MBasicBlock* block)
    {
        uint32_t numPushed = block->stackDepth() - block->info().firstStackSlot();
        MOZ_ASSERT(numPushed == 0 || numPushed == 1);
        return numPushed;
    }

    static void push(MBasicBlock* block, MDefinition* def)
    {
        MOZ_ASSERT(!hasPushed(block));
        block->push(def);
    }

    static void popAll(BlockVector* blocks)
    {
        for (MBasicBlock* block : *blocks)
            block->pop();
    }

  public:
    bool addJoinPredecessor(MDefinition* def, BlockVector* blocks)
    {
        if (inDeadCode())
            return true;

        // Preserve the invariant that, for every MBasicBlock in 'blocks',
        // either: every MBasicBlock has a non-void pushed expression OR no
        // MBasicBlock has any pushed expression. This is required by
        // MBasicBlock::addPredecessor.
        if (def) {
            if (blocks->empty()) {
                if (def->type() != MIRType_None)
                    push(curBlock_, def);
            } else {
                if (hasPushed((*blocks)[0])) {
                    if (def->type() == MIRType_None)
                        popAll(blocks);
                    else
                        push(curBlock_, def);
                }
            }
        } else {
            if (!blocks->empty() && hasPushed((*blocks)[0]))
                popAll(blocks);
        }

        return blocks->append(curBlock_);
    }

    bool joinIf(MBasicBlock* joinBlock, BlockVector* blocks, MDefinition** def)
    {
        MOZ_ASSERT_IF(curBlock_, blocks->back() == curBlock_);
        curBlock_ = joinBlock;
        return joinIfElse(nullptr, blocks, def);
    }

    void switchToElse(MBasicBlock* elseBlock)
    {
        if (!elseBlock)
            return;
        curBlock_ = elseBlock;
        mirGraph().moveBlockToEnd(curBlock_);
    }

    bool joinIfElse(MDefinition* elseDef, BlockVector* blocks, MDefinition** def)
    {
        if (!addJoinPredecessor(elseDef, blocks))
            return false;

        if (blocks->empty()) {
            *def = nullptr;
            return true;
        }

        MBasicBlock* join;
        if (!goToNewBlock((*blocks)[0], &join))
            return false;
        for (size_t i = 1; i < blocks->length(); i++) {
            if (!goToExistingBlock((*blocks)[i], join))
                return false;
        }

        curBlock_ = join;
        if (hasPushed(curBlock_))
            *def = curBlock_->pop();
        else
            *def = nullptr;
        return true;
    }

    bool startBlock()
    {
        MOZ_ASSERT_IF(blockDepth_ < targets_.length(), targets_[blockDepth_].empty());
        blockDepth_++;
        return true;
    }

    bool finishBlock()
    {
        MOZ_ASSERT(blockDepth_);
        uint32_t topLabel = --blockDepth_;
        return bindBranches(topLabel);
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

    bool setLoopBackedge(MBasicBlock* loopEntry, MBasicBlock* loopBody, MBasicBlock* backedge,
                         MDefinition** loopResult)
    {
        if (!loopEntry->setBackedgeAsmJS(backedge))
            return false;

        // Flag all redundant phis as unused.
        for (MPhiIterator phi = loopEntry->phisBegin(); phi != loopEntry->phisEnd(); phi++) {
            MOZ_ASSERT(phi->numOperands() == 2);
            if (phi->getOperand(0) == phi->getOperand(1))
                phi->setUnused();
        }

        // The loop result may also be referencing a recycled phi.
        if (*loopResult && (*loopResult)->isUnused())
            *loopResult = (*loopResult)->toPhi()->getOperand(0);

        // Fix up phis stored in the slots Vector of pending blocks.
        for (BlockVector& vec : targets_) {
            for (MBasicBlock* block : vec) {
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
            MOZ_ASSERT(afterLabel >= targets_.length() || targets_[afterLabel].empty());
            MOZ_ASSERT(headerLabel >= targets_.length() || targets_[headerLabel].empty());
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
        if (!bindBranches(headerLabel))
            return false;

        MOZ_ASSERT(loopHeader->loopDepth() == loopDepth_);

        if (curBlock_) {
            // We're on the loop backedge block, created by bindBranches.
            MOZ_ASSERT(curBlock_->loopDepth() == loopDepth_);
            curBlock_->end(MGoto::New(alloc(), loopHeader));
            if (!setLoopBackedge(loopHeader, loopBody, curBlock_, loopResult))
                return false;
        }

        curBlock_ = loopBody;

        loopDepth_--;
        if (!bindBranches(afterLabel))
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

    bool startSwitch(MDefinition* expr, uint32_t numCases, MBasicBlock** switchBlock)
    {
        if (inDeadCode()) {
            *switchBlock = nullptr;
            return true;
        }
        MOZ_ASSERT(numCases <= INT32_MAX);
        MOZ_ASSERT(numCases);
        curBlock_->end(MTableSwitch::New(alloc(), expr, 0, int32_t(numCases - 1)));
        *switchBlock = curBlock_;
        curBlock_ = nullptr;
        return true;
    }

    bool startSwitchCase(MBasicBlock* switchBlock, MBasicBlock** next)
    {
        MOZ_ASSERT(inDeadCode());
        if (!switchBlock) {
            *next = nullptr;
            return true;
        }
        if (!newBlock(switchBlock, next))
            return false;
        curBlock_ = *next;
        return true;
    }

    bool joinSwitch(MBasicBlock* switchBlock, const BlockVector& cases, MBasicBlock* defaultBlock)
    {
        MOZ_ASSERT(inDeadCode());
        if (!switchBlock)
            return true;

        MTableSwitch* mir = switchBlock->lastIns()->toTableSwitch();
        size_t defaultIndex;
        if (!mir->addDefault(defaultBlock, &defaultIndex))
            return false;

        for (MBasicBlock* caseBlock : cases) {
            if (!caseBlock) {
                if (!mir->addCase(defaultIndex))
                    return false;
            } else {
                size_t caseIndex;
                if (!mir->addSuccessor(caseBlock, &caseIndex))
                    return false;
                if (!mir->addCase(caseIndex))
                    return false;
            }
        }

        return true;
    }

    bool br(uint32_t relativeDepth)
    {
        if (inDeadCode())
            return true;

        MOZ_ASSERT(relativeDepth < blockDepth_);
        uint32_t absolute = blockDepth_ - 1 - relativeDepth;
        if (absolute >= targets_.length()) {
            if (!targets_.resize(absolute + 1))
                return false;
        }

        if (!targets_[absolute].append(curBlock_))
            return false;

        curBlock_ = nullptr;
        return true;
    }

    bool brIf(uint32_t relativeDepth, MDefinition* condition)
    {
        if (inDeadCode())
            return true;

        // TODO (bug 1253334): we could use MTest with the right jump target,
        // here. If it's backward, it's trivial; if it's forward, we need to
        // memorize it, then fix it later when we actually encounter the target.
        MBasicBlock* thenBlock = nullptr;
        MBasicBlock* joinBlock = nullptr;
        if (!newBlock(curBlock_, &thenBlock))
            return false;
        if (!newBlock(curBlock_, &joinBlock))
            return false;

        curBlock_->end(MTest::New(alloc(), condition, thenBlock, joinBlock));
        curBlock_ = thenBlock;
        mirGraph().moveBlockToEnd(curBlock_);

        if (!br(relativeDepth))
            return false;

        MOZ_ASSERT(inDeadCode());
        curBlock_ = joinBlock;
        return true;
    }

    /************************************************************ DECODING ***/

    uint8_t        readU8()       { return decoder_.uncheckedReadFixedU8(); }
    uint32_t       readU32()      { return decoder_.uncheckedReadFixedU32(); }
    uint32_t       readVarS32()   { return decoder_.uncheckedReadVarS32(); }
    uint32_t       readVarU32()   { return decoder_.uncheckedReadVarU32(); }
    uint64_t       readVarU64()   { return decoder_.uncheckedReadVarU64(); }
    uint64_t       readVarS64()   { return decoder_.uncheckedReadVarS64(); }
    float          readF32()      { return decoder_.uncheckedReadFixedF32(); }
    double         readF64()      { return decoder_.uncheckedReadFixedF64(); }
    Expr           readExpr()     { return decoder_.uncheckedReadExpr(); }
    Expr           peakExpr()     { return decoder_.uncheckedPeekExpr(); }

    SimdConstant readI32X4() {
        I32x4 i32x4;
        JS_ALWAYS_TRUE(decoder_.readFixedI32x4(&i32x4));
        return SimdConstant::CreateX4(i32x4);
    }
    SimdConstant readF32X4() {
        F32x4 f32x4;
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

    bool bindBranches(uint32_t absolute)
    {
        if (absolute >= targets_.length() || targets_[absolute].empty())
            return true;

        BlockVector& preds = targets_[absolute];

        MBasicBlock* join;
        if (!goToNewBlock(preds[0], &join))
            return false;
        for (size_t i = 1; i < preds.length(); i++) {
            if (!mirGen_.ensureBallast())
                return false;
            if (!goToExistingBlock(preds[i], join))
                return false;
        }

        if (curBlock_ && !goToExistingBlock(curBlock_, join))
            return false;
        curBlock_ = join;

        preds.clear();
        return true;
    }
};

static bool
EmitLiteral(FunctionCompiler& f, ValType type, MDefinition** def)
{
    switch (type) {
      case ValType::I32: {
        int32_t val = f.readVarS32();
        *def = f.constant(Int32Value(val), MIRType_Int32);
        return true;
      }
      case ValType::I64: {
        int64_t val = f.readVarS64();
        *def = f.constant(val);
        return true;
      }
      case ValType::F32: {
        float val = f.readF32();
        *def = f.constant(Float32Value(val), MIRType_Float32);
        return true;
      }
      case ValType::F64: {
        double val = f.readF64();
        *def = f.constant(DoubleValue(val), MIRType_Double);
        return true;
      }
      case ValType::I32x4: {
        SimdConstant lit(f.readI32X4());
        *def = f.constant(lit, MIRType_Int32x4);
        return true;
      }
      case ValType::F32x4: {
        SimdConstant lit(f.readF32X4());
        *def = f.constant(lit, MIRType_Float32x4);
        return true;
      }
      case ValType::B32x4: {
        // Boolean vectors are stored as an Int vector with -1 / 0 lanes.
        SimdConstant lit(f.readI32X4());
        *def = f.constant(lit, MIRType_Bool32x4);
        return true;
      }
      case ValType::Limit:
        break;
    }
    MOZ_CRASH("unexpected literal type");
}

static bool
EmitGetLocal(FunctionCompiler& f, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    *def = f.getLocalDef(slot);
    return true;
}

static bool
EmitLoadGlobal(FunctionCompiler& f, MDefinition** def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    *def = f.loadGlobalVar(global.globalDataOffset, global.isConst, ToMIRType(global.type));
    return true;
}

static bool EmitExpr(FunctionCompiler&, MDefinition**);

static bool
EmitHeapAddress(FunctionCompiler& f, MDefinition** base, MAsmJSHeapAccess* access)
{
    uint32_t alignLog2 = f.readVarU32();
    access->setAlign(1 << alignLog2);

    uint32_t offset = f.readVarU32();
    access->setOffset(offset);

    if (!EmitExpr(f, base))
        return false;

    if (f.mg().isAsmJS()) {
        MOZ_ASSERT(offset == 0 && "asm.js validation does not produce load/store offsets");
        return true;
    }

    // TODO Remove this after implementing non-wraparound offset semantics.
    uint32_t endOffset = access->endOffset();
    if (endOffset < offset)
        return false;
    bool accessNeedsBoundsCheck = true;
    // Assume worst case.
    bool atomicAccess = true;
    if (endOffset > f.mirGen().foldableOffsetRange(accessNeedsBoundsCheck, atomicAccess)) {
        MDefinition* rhs = f.constant(Int32Value(offset), MIRType_Int32);
        *base = f.binary<MAdd>(*base, rhs, MIRType_Int32);
        offset = 0;
        access->setOffset(offset);
    }

    return true;
}

static bool
EmitLoad(FunctionCompiler& f, Scalar::Type viewType, MDefinition** def)
{
    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;
    *def = f.loadHeap(base, access);
    return true;
}

static bool
EmitStore(FunctionCompiler& f, Scalar::Type viewType, MDefinition** def)
{
    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* rhs = nullptr;
    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
        if (!EmitExpr(f, &rhs))
            return false;
        break;
      case Scalar::Float32:
        if (!EmitExpr(f, &rhs))
            return false;
        break;
      case Scalar::Float64:
        if (!EmitExpr(f, &rhs))
            return false;
        break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.storeHeap(base, access, rhs);
    *def = rhs;
    return true;
}

static bool
EmitStoreWithCoercion(FunctionCompiler& f, Scalar::Type rhsType, Scalar::Type viewType,
                      MDefinition **def)
{
    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* rhs = nullptr;
    MDefinition* coerced = nullptr;
    if (rhsType == Scalar::Float32 && viewType == Scalar::Float64) {
        if (!EmitExpr(f, &rhs))
            return false;
        coerced = f.unary<MToDouble>(rhs);
    } else if (rhsType == Scalar::Float64 && viewType == Scalar::Float32) {
        if (!EmitExpr(f, &rhs))
            return false;
        coerced = f.unary<MToFloat32>(rhs);
    } else {
        MOZ_CRASH("unexpected coerced store");
    }

    f.storeHeap(base, access, coerced);
    *def = rhs;
    return true;
}

static bool
EmitSetLocal(FunctionCompiler& f, MDefinition** def)
{
    uint32_t slot = f.readVarU32();
    MDefinition* expr;
    if (!EmitExpr(f, &expr))
        return false;
    f.assign(slot, expr);
    *def = expr;
    return true;
}

static bool
EmitStoreGlobal(FunctionCompiler& f, MDefinition**def)
{
    uint32_t index = f.readVarU32();
    const AsmJSGlobalVariable& global = f.mg().globalVar(index);
    MDefinition* expr;
    if (!EmitExpr(f, &expr))
        return false;
    f.storeGlobalVar(global.globalDataOffset, expr);
    *def = expr;
    return true;
}

typedef bool IsMax;

static bool
EmitMathMinMax(FunctionCompiler& f, ValType type, bool isMax, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.minMax(lhs, rhs, mirType, isMax);
    return true;
}

static bool
EmitAtomicsLoad(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    MDefinition* base;
    MAsmJSHeapAccess access(viewType, 0, MembarBeforeLoad, MembarAfterLoad);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    *def = f.atomicLoadHeap(base, access);
    return true;
}

static bool
EmitAtomicsStore(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    MDefinition* base;
    MAsmJSHeapAccess access(viewType, 0, MembarBeforeStore, MembarAfterStore);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, &value))
        return false;
    f.atomicStoreHeap(base, access, value);
    *def = value;
    return true;
}

static bool
EmitAtomicsBinOp(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());
    js::jit::AtomicOp op = js::jit::AtomicOp(f.readU8());

    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, &value))
        return false;
    *def = f.atomicBinopHeap(op, base, access, value);
    return true;
}

static bool
EmitAtomicsCompareExchange(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* oldValue;
    if (!EmitExpr(f, &oldValue))
        return false;
    MDefinition* newValue;
    if (!EmitExpr(f, &newValue))
        return false;
    *def = f.atomicCompareExchangeHeap(base, access, oldValue, newValue);
    return true;
}

static bool
EmitAtomicsExchange(FunctionCompiler& f, MDefinition** def)
{
    Scalar::Type viewType = Scalar::Type(f.readU8());

    MDefinition* base;
    MAsmJSHeapAccess access(viewType);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* value;
    if (!EmitExpr(f, &value))
        return false;
    *def = f.atomicExchangeHeap(base, access, value);
    return true;
}

static bool
EmitCallArgs(FunctionCompiler& f, const Sig& sig, FunctionCompiler::Call* call)
{
    f.startCallArgs(call);
    for (ValType argType : sig.args()) {
        MDefinition* arg;
        if (!EmitExpr(f, &arg))
            return false;
        if (!f.passArg(arg, argType, call))
            return false;
    }
    f.finishCallArgs(call);
    return true;
}

static bool
EmitCall(FunctionCompiler& f, uint32_t callOffset, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t funcIndex = f.readVarU32();

    const Sig& sig = f.mg().funcSig(funcIndex);

    FunctionCompiler::Call call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    return f.internalCall(sig, funcIndex, call, def);
}

static bool
EmitCallIndirect(FunctionCompiler& f, uint32_t callOffset, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t sigIndex = f.readVarU32();

    const Sig& sig = f.mg().sig(sigIndex);

    MDefinition* index;
    if (!EmitExpr(f, &index))
        return false;

    FunctionCompiler::Call call(f, lineOrBytecode);
    if (!EmitCallArgs(f, sig, &call))
        return false;

    const TableModuleGeneratorData& table = f.mg().sigToTable(sigIndex);
    return f.funcPtrCall(sig, table.numElems, table.globalDataOffset, index, call, def);
}

static bool
EmitCallImport(FunctionCompiler& f, uint32_t callOffset, MDefinition** def)
{
    uint32_t lineOrBytecode = f.readCallSiteLineOrBytecode(callOffset);
    uint32_t importIndex = f.readVarU32();

    const ImportModuleGeneratorData& import = f.mg().import(importIndex);
    const Sig& sig = *import.sig;

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
    if (!EmitExpr(f, &firstArg) || !f.passArg(firstArg, ValType::F32, &call))
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
    if (!EmitExpr(f, &firstArg) || !f.passArg(firstArg, ValType::F64, &call))
        return false;

    if (f64 == Expr::F64Pow || f64 == Expr::F64Atan2) {
        MDefinition* secondArg;
        if (!EmitExpr(f, &secondArg) || !f.passArg(secondArg, ValType::F64, &call))
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
EmitSimdUnary(FunctionCompiler& f, ValType type, SimdOperation simdOp, MDefinition** def)
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
    if (!EmitExpr(f, &in))
        return false;
    *def = f.unarySimd(in, op, ToMIRType(type));
    return true;
}

template<class OpKind>
inline bool
EmitSimdBinary(FunctionCompiler& f, ValType type, OpKind op, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    *def = f.binarySimd(lhs, rhs, op, ToMIRType(type));
    return true;
}

static bool
EmitSimdBinaryComp(FunctionCompiler& f, MSimdBinaryComp::Operation op, SimdSign sign,
                   MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    *def = f.binarySimdComp(lhs, rhs, op, sign);
    return true;
}

static bool
EmitSimdShift(FunctionCompiler& f, MSimdShift::Operation op, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    *def = f.binarySimd<MSimdShift>(lhs, rhs, op);
    return true;
}

static ValType
SimdToLaneType(ValType type)
{
    switch (type) {
      case ValType::I32x4:  return ValType::I32;
      case ValType::F32x4:  return ValType::F32;
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
EmitExtractLane(FunctionCompiler& f, ValType type, SimdSign sign, MDefinition** def)
{
    MDefinition* vec;
    if (!EmitExpr(f, &vec))
        return false;

    MDefinition* laneDef;
    if (!EmitExpr(f, &laneDef))
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
    if (!EmitExpr(f, &i32))
        return false;
    // Now compute !i32 - 1 to force the value range into {0, -1}.
    MDefinition* noti32 = f.unary<MNot>(i32);
    *def = f.binary<MSub>(noti32, f.constant(Int32Value(1), MIRType_Int32), MIRType_Int32);
    return true;
}

static bool
EmitSimdReplaceLane(FunctionCompiler& f, ValType simdType, MDefinition** def)
{
    MDefinition* vector;
    if (!EmitExpr(f, &vector))
        return false;

    MDefinition* laneDef;
    if (!EmitExpr(f, &laneDef))
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
    } else {
        if (!EmitExpr(f, &scalar))
            return false;
    }
    *def = f.insertElementSimd(vector, scalar, lane, ToMIRType(simdType));
    return true;
}

inline bool
EmitSimdBitcast(FunctionCompiler& f, ValType fromType, ValType toType, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.bitcastSimd(in, ToMIRType(fromType), ToMIRType(toType));
    return true;
}

inline bool
EmitSimdConvert(FunctionCompiler& f, ValType fromType, ValType toType, SimdSign sign,
                MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.convertSimd(in, ToMIRType(fromType), ToMIRType(toType), sign);
    return true;
}

static bool
EmitSimdSwizzle(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;

    uint8_t lanes[4];
    for (unsigned i = 0; i < 4; i++)
        lanes[i] = f.readU8();

    *def = f.swizzleSimd(in, lanes[0], lanes[1], lanes[2], lanes[3], ToMIRType(type));
    return true;
}

static bool
EmitSimdShuffle(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;

    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;

    uint8_t lanes[4];
    for (unsigned i = 0; i < 4; i++)
        lanes[i] = f.readU8();

    *def = f.shuffleSimd(lhs, rhs, lanes[0], lanes[1], lanes[2], lanes[3], ToMIRType(type));
    return true;
}

static inline Scalar::Type
SimdExprTypeToViewType(ValType type, unsigned* defaultNumElems)
{
    switch (type) {
        case ValType::I32x4: *defaultNumElems = 4; return Scalar::Int32x4;
        case ValType::F32x4: *defaultNumElems = 4; return Scalar::Float32x4;
        default:              break;
    }
    MOZ_CRASH("type not handled in SimdExprTypeToViewType");
}

static bool
EmitSimdLoad(FunctionCompiler& f, ValType type, unsigned numElems, MDefinition** def)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(type, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    MDefinition* base;
    MAsmJSHeapAccess access(viewType, numElems);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    *def = f.loadSimdHeap(base, access);
    return true;
}

static bool
EmitSimdStore(FunctionCompiler& f, ValType type, unsigned numElems, MDefinition** def)
{
    unsigned defaultNumElems;
    Scalar::Type viewType = SimdExprTypeToViewType(type, &defaultNumElems);

    if (!numElems)
        numElems = defaultNumElems;

    MDefinition* base;
    MAsmJSHeapAccess access(viewType, numElems);
    if (!EmitHeapAddress(f, &base, &access))
        return false;

    MDefinition* vec;
    if (!EmitExpr(f, &vec))
        return false;

    f.storeSimdHeap(base, access, vec);
    *def = vec;
    return true;
}

static bool
EmitSimdSelect(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* mask;
    MDefinition* defs[2];

    // The mask is a boolean vector for elementwise select.
    if (!EmitExpr(f, &mask))
        return false;

    if (!EmitExpr(f, &defs[0]) || !EmitExpr(f, &defs[1]))
        return false;
    *def = f.selectSimd(mask, defs[0], defs[1], ToMIRType(type));
    return true;
}

static bool
EmitSimdAllTrue(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.simdAllTrue(in);
    return true;
}

static bool
EmitSimdAnyTrue(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.simdAnyTrue(in);
    return true;
}

static bool
EmitSimdSplat(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* in;
    if (IsSimdBoolType(type)) {
        if (!EmitSimdBooleanLaneExpr(f, &in))
            return false;
    } else {
        if (!EmitExpr(f, &in))
            return false;
    }
    *def = f.splatSimd(in, ToMIRType(type));
    return true;
}

static bool
EmitSimdCtor(FunctionCompiler& f, ValType type, MDefinition** def)
{
    switch (type) {
      case ValType::I32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitExpr(f, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Int32x4);
        return true;
      }
      case ValType::F32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitExpr(f, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Float32x4);
        return true;
      }
      case ValType::B32x4: {
        MDefinition* args[4];
        for (unsigned i = 0; i < 4; i++) {
            if (!EmitSimdBooleanLaneExpr(f, &args[i]))
                return false;
        }
        *def = f.constructSimd<MSimdValueX4>(args[0], args[1], args[2], args[3], MIRType_Bool32x4);
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

template<class T>
static bool
EmitUnary(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.unary<T>(in);
    return true;
}

template<class T>
static bool
EmitUnaryWithType(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.unary<T>(in, ToMIRType(type));
    return true;
}

static bool
EmitMultiply(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.mul(lhs, rhs, mirType, mirType == MIRType_Int32 ? MMul::Integer : MMul::Normal);
    return true;
}

static bool
EmitSelect(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* trueExpr;
    if (!EmitExpr(f, &trueExpr))
        return false;

    MDefinition* falseExpr;
    if (!EmitExpr(f, &falseExpr))
        return false;

    MDefinition* condExpr;
    if (!EmitExpr(f, &condExpr))
        return false;

    *def = f.select(trueExpr, falseExpr, condExpr);
    return true;
}

typedef bool IsAdd;

static bool
EmitAddOrSub(FunctionCompiler& f, ValType type, bool isAdd, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = isAdd ? f.binary<MAdd>(lhs, rhs, mirType) : f.binary<MSub>(lhs, rhs, mirType);
    return true;
}

typedef bool IsUnsigned;
typedef bool IsDiv;

static bool
EmitDivOrMod(FunctionCompiler& f, ValType type, bool isDiv, bool isUnsigned, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    *def = isDiv
           ? f.div(lhs, rhs, ToMIRType(type), isUnsigned)
           : f.mod(lhs, rhs, ToMIRType(type), isUnsigned);
    return true;
}

static bool
EmitDivOrMod(FunctionCompiler& f, ValType type, bool isDiv, MDefinition** def)
{
    MOZ_ASSERT(type != ValType::I32 && type != ValType::I64,
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
        if (!EmitExpr(f, &lhs) || !EmitExpr(f, &rhs))
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
        if (!EmitExpr(f, &lhs) || !EmitExpr(f, &rhs))
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
        if (!EmitExpr(f, &lhs) || !EmitExpr(f, &rhs))
            return false;
        compareType = MCompare::Compare_Float32;
        break;
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Le:
      case Expr::F64Lt:
      case Expr::F64Ge:
      case Expr::F64Gt:
        if (!EmitExpr(f, &lhs) || !EmitExpr(f, &rhs))
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
EmitBitwise(FunctionCompiler& f, ValType type, MDefinition** def)
{
    MDefinition* lhs;
    if (!EmitExpr(f, &lhs))
        return false;
    MDefinition* rhs;
    if (!EmitExpr(f, &rhs))
        return false;
    MIRType mirType = ToMIRType(type);
    *def = f.bitwise<T>(lhs, rhs, mirType);
    return true;
}

static bool
EmitBitwiseNot(FunctionCompiler& f, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.bitwise<MBitNot>(in);
    return true;
}

static bool
EmitExtendI32(FunctionCompiler& f, bool isUnsigned, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.extendI32(in, isUnsigned);
    return true;
}

template<class T>
static bool
EmitTruncate(FunctionCompiler& f, bool isUnsigned, MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.truncate<T>(in, isUnsigned);
    return true;
}

static bool
EmitConvertI64ToFloatingPoint(FunctionCompiler& f, ValType type, bool isUnsigned,
                              MDefinition** def)
{
    MDefinition* in;
    if (!EmitExpr(f, &in))
        return false;
    *def = f.convertI64ToFloatingPoint(in, ToMIRType(type), isUnsigned);
    return true;
}

static bool
EmitSimdOp(FunctionCompiler& f, ValType type, SimdOperation op, SimdSign sign, MDefinition** def)
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
        return EmitSimdAllTrue(f, def);
      case SimdOperation::Fn_anyTrue:
        return EmitSimdAnyTrue(f, def);
      case SimdOperation::Fn_abs:
      case SimdOperation::Fn_neg:
      case SimdOperation::Fn_not:
      case SimdOperation::Fn_sqrt:
      case SimdOperation::Fn_reciprocalApproximation:
      case SimdOperation::Fn_reciprocalSqrtApproximation:
        return EmitSimdUnary(f, type, op, def);
      case SimdOperation::Fn_shiftLeftByScalar:
        return EmitSimdShift(f, MSimdShift::lsh, def);
      case SimdOperation::Fn_shiftRightByScalar:
        return EmitSimdShift(f, MSimdShift::rshForSign(sign), def);
#define _CASE(OP) \
      case SimdOperation::Fn_##OP: \
        return EmitSimdBinaryComp(f, MSimdBinaryComp::OP, sign, def);
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
        return EmitSimdConvert(f, ValType::F32x4, type, sign, def);
      case SimdOperation::Fn_fromInt32x4:
        return EmitSimdConvert(f, ValType::I32x4, type, SimdSign::Signed, def);
      case SimdOperation::Fn_fromUint32x4:
        return EmitSimdConvert(f, ValType::I32x4, type, SimdSign::Unsigned, def);
      case SimdOperation::Fn_fromInt32x4Bits:
      case SimdOperation::Fn_fromUint32x4Bits:
        return EmitSimdBitcast(f, ValType::I32x4, type, def);
      case SimdOperation::Fn_fromFloat32x4Bits:
      case SimdOperation::Fn_fromInt8x16Bits:
        return EmitSimdBitcast(f, ValType::F32x4, type, def);
      case SimdOperation::Fn_fromInt16x8Bits:
      case SimdOperation::Fn_fromUint8x16Bits:
      case SimdOperation::Fn_fromUint16x8Bits:
      case SimdOperation::Fn_fromFloat64x2Bits:
        MOZ_CRASH("NYI");
    }
    MOZ_CRASH("unexpected opcode");
}

static bool
EmitLoop(FunctionCompiler& f, MDefinition** def)
{
    MBasicBlock* loopHeader;
    if (!f.startLoop(&loopHeader))
        return false;

    f.addInterruptCheck();

    if (uint32_t numStmts = f.readVarU32()) {
        for (uint32_t i = 0; i < numStmts - 1; i++) {
            MDefinition* _;
            if (!EmitExpr(f, &_))
                return false;
        }
        if (!EmitExpr(f, def))
            return false;
    } else {
        *def = nullptr;
    }

    return f.closeLoop(loopHeader, def);
}

static bool
EmitIfElse(FunctionCompiler& f, Expr op, MDefinition** def)
{
    MOZ_ASSERT(op == Expr::If || op == Expr::IfElse);

    // Handle if/else-if chains using iteration instead of recursion. This
    // avoids blowing the C stack quota for long if/else-if chains and also
    // creates fewer MBasicBlocks at join points (by creating one join block
    // for the entire if/else-if chain).
    BlockVector blocks;

  recurse:
    MDefinition* condition;
    if (!EmitExpr(f, &condition))
        return false;

    MBasicBlock* thenBlock = nullptr;
    MBasicBlock* elseOrJoinBlock = nullptr;
    if (!f.branchAndStartThen(condition, &thenBlock, &elseOrJoinBlock))
        return false;

    MDefinition* ifDef;
    if (!EmitExpr(f, &ifDef))
        return false;

    if (!f.addJoinPredecessor(ifDef, &blocks))
        return false;

    if (op == Expr::If)
        return f.joinIf(elseOrJoinBlock, &blocks, def);

    f.switchToElse(elseOrJoinBlock);

    Expr nextStmt = f.peakExpr();
    if (nextStmt == Expr::If || nextStmt == Expr::IfElse) {
        JS_ALWAYS_TRUE(f.readExpr() == nextStmt);
        op = nextStmt;
        goto recurse;
    }

    MDefinition* elseDef;
    if (!EmitExpr(f, &elseDef))
        return false;

    return f.joinIfElse(elseDef, &blocks, def);
}

static bool
EmitBrTable(FunctionCompiler& f, MDefinition** def)
{
    uint32_t numCases = f.readVarU32();

    BlockVector cases;
    if (!cases.resize(numCases))
        return false;

    Uint32Vector depths;
    if (!depths.resize(numCases))
        return false;

    for (size_t i = 0; i < numCases; i++)
        depths[i] = f.readU32();

    uint32_t defaultDepth = f.readU32();

    MDefinition* index;
    if (!EmitExpr(f, &index))
        return false;

    *def = nullptr;

    // Empty table
    if (!numCases)
        return f.br(defaultDepth);

    MBasicBlock* switchBlock;
    if (!f.startSwitch(index, numCases, &switchBlock))
        return false;

    MBasicBlock* defaultBlock = nullptr;
    if (!f.startSwitchCase(switchBlock, &defaultBlock))
        return false;
    if (!f.br(defaultDepth))
        return false;

    // TODO (bug 1253334): we could avoid one indirection here, by
    // jump-threading by hand the jump to the right enclosing block.
    for (uint32_t i = 0; i < numCases; i++) {
        uint32_t depth = depths[i];
        // Don't emit blocks for the default case, to reduce the number of
        // MBasicBlocks created.
        if (depth == defaultDepth)
            continue;
        if (!f.startSwitchCase(switchBlock, &cases[i]))
            return false;
        if (!f.br(depth))
            return false;
    }

    if (!f.joinSwitch(switchBlock, cases, defaultBlock))
        return false;

    return true;
}

static bool
EmitReturn(FunctionCompiler& f, MDefinition** def)
{
    ExprType ret = f.sig().ret();

    if (IsVoid(ret)) {
        *def = nullptr;
        f.returnVoid();
        return true;
    }

    MDefinition* retVal;
    if (!EmitExpr(f, &retVal))
        return false;

    f.returnExpr(retVal);

    *def = nullptr;
    return true;
}

static bool
EmitUnreachable(FunctionCompiler& f, MDefinition** def)
{
    *def = nullptr;
    return f.unreachableTrap();
}

static bool
EmitBlock(FunctionCompiler& f, MDefinition** def)
{
    if (!f.startBlock())
        return false;
    if (uint32_t numStmts = f.readVarU32()) {
        for (uint32_t i = 0; i < numStmts - 1; i++) {
            MDefinition* _;
            if (!EmitExpr(f, &_))
                return false;
        }
        if (!EmitExpr(f, def))
            return false;
    } else {
        *def = nullptr;
    }
    return f.finishBlock();
}

static bool
EmitBranch(FunctionCompiler& f, Expr op, MDefinition** def)
{
    MOZ_ASSERT(op == Expr::Br || op == Expr::BrIf);

    uint32_t relativeDepth = f.readVarU32();

    MOZ_ALWAYS_TRUE(f.readExpr() == Expr::Nop);

    if (op == Expr::Br) {
        if (!f.br(relativeDepth))
            return false;
    } else {
        MDefinition* condition;
        if (!EmitExpr(f, &condition))
            return false;

        if (!f.brIf(relativeDepth, condition))
            return false;
    }

    *def = nullptr;
    return true;
}

static bool
EmitExpr(FunctionCompiler& f, MDefinition** def)
{
    if (!f.mirGen().ensureBallast())
        return false;

    uint32_t exprOffset = f.currentOffset();

    switch (Expr op = f.readExpr()) {
      // Control opcodes
      case Expr::Nop:
        *def = nullptr;
        return true;
      case Expr::Id:
        return EmitExpr(f, def);
      case Expr::Block:
        return EmitBlock(f, def);
      case Expr::If:
      case Expr::IfElse:
        return EmitIfElse(f, op, def);
      case Expr::Loop:
        return EmitLoop(f, def);
      case Expr::Br:
      case Expr::BrIf:
        return EmitBranch(f, op, def);
      case Expr::BrTable:
        return EmitBrTable(f, def);
      case Expr::Return:
        return EmitReturn(f, def);
      case Expr::Unreachable:
        return EmitUnreachable(f, def);

      // Calls
      case Expr::Call:
        return EmitCall(f, exprOffset, def);
      case Expr::CallIndirect:
        return EmitCallIndirect(f, exprOffset, def);
      case Expr::CallImport:
        return EmitCallImport(f, exprOffset, def);

      // Locals and globals
      case Expr::GetLocal:
        return EmitGetLocal(f, def);
      case Expr::SetLocal:
        return EmitSetLocal(f, def);
      case Expr::LoadGlobal:
        return EmitLoadGlobal(f, def);
      case Expr::StoreGlobal:
        return EmitStoreGlobal(f, def);

      // Select
      case Expr::Select:
        return EmitSelect(f, def);

      // I32
      case Expr::I32Const:
        return EmitLiteral(f, ValType::I32, def);
      case Expr::I32Add:
        return EmitAddOrSub(f, ValType::I32, IsAdd(true), def);
      case Expr::I32Sub:
        return EmitAddOrSub(f, ValType::I32, IsAdd(false), def);
      case Expr::I32Mul:
        return EmitMultiply(f, ValType::I32, def);
      case Expr::I32DivS:
      case Expr::I32DivU:
        return EmitDivOrMod(f, ValType::I32, IsDiv(true), IsUnsigned(op == Expr::I32DivU), def);
      case Expr::I32RemS:
      case Expr::I32RemU:
        return EmitDivOrMod(f, ValType::I32, IsDiv(false), IsUnsigned(op == Expr::I32RemU), def);
      case Expr::I32Min:
        return EmitMathMinMax(f, ValType::I32, IsMax(false), def);
      case Expr::I32Max:
        return EmitMathMinMax(f, ValType::I32, IsMax(true), def);
      case Expr::I32Eqz:
        return EmitUnary<MNot>(f, def);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
        return EmitUnary<MTruncateToInt32>(f, def);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return EmitUnary<MTruncateToInt32>(f, def);
      case Expr::I32WrapI64:
        return EmitUnary<MWrapInt64ToInt32>(f, def);
      case Expr::I32Clz:
        return EmitUnary<MClz>(f, def);
      case Expr::I32Ctz:
        return EmitUnary<MCtz>(f, def);
      case Expr::I32Popcnt:
        return EmitUnary<MPopcnt>(f, def);
      case Expr::I32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::I32, def);
      case Expr::I32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::I32, def);
      case Expr::I32Or:
        return EmitBitwise<MBitOr>(f, ValType::I32, def);
      case Expr::I32And:
        return EmitBitwise<MBitAnd>(f, ValType::I32, def);
      case Expr::I32Xor:
        return EmitBitwise<MBitXor>(f, ValType::I32, def);
      case Expr::I32Shl:
        return EmitBitwise<MLsh>(f, ValType::I32, def);
      case Expr::I32ShrS:
        return EmitBitwise<MRsh>(f, ValType::I32, def);
      case Expr::I32ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I32, def);
      case Expr::I32BitNot:
        return EmitBitwiseNot(f, def);
      case Expr::I32Load8S:
        return EmitLoad(f, Scalar::Int8, def);
      case Expr::I32Load8U:
        return EmitLoad(f, Scalar::Uint8, def);
      case Expr::I32Load16S:
        return EmitLoad(f, Scalar::Int16, def);
      case Expr::I32Load16U:
        return EmitLoad(f, Scalar::Uint16, def);
      case Expr::I32Load:
        return EmitLoad(f, Scalar::Int32, def);
      case Expr::I32Store8:
        return EmitStore(f, Scalar::Int8, def);
      case Expr::I32Store16:
        return EmitStore(f, Scalar::Int16, def);
      case Expr::I32Store:
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

      // I64
      case Expr::I64Const:
        return EmitLiteral(f, ValType::I64, def);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return EmitExtendI32(f, IsUnsigned(op == Expr::I64ExtendUI32), def);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return EmitTruncate<MTruncateToInt64>(f, IsUnsigned(op == Expr::I64TruncUF32), def);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
        return EmitTruncate<MTruncateToInt64>(f, IsUnsigned(op == Expr::I64TruncUF64), def);
      case Expr::I64Or:
        return EmitBitwise<MBitOr>(f, ValType::I64, def);
      case Expr::I64And:
        return EmitBitwise<MBitAnd>(f, ValType::I64, def);
      case Expr::I64Xor:
        return EmitBitwise<MBitXor>(f, ValType::I64, def);
      case Expr::I64Shl:
        return EmitBitwise<MLsh>(f, ValType::I64, def);
      case Expr::I64ShrS:
        return EmitBitwise<MRsh>(f, ValType::I64, def);
      case Expr::I64ShrU:
        return EmitBitwise<MUrsh>(f, ValType::I64, def);
      case Expr::I64Add:
        return EmitAddOrSub(f, ValType::I64, IsAdd(true), def);
      case Expr::I64Sub:
        return EmitAddOrSub(f, ValType::I64, IsAdd(false), def);
      case Expr::I64Mul:
        return EmitMultiply(f, ValType::I64, def);
      case Expr::I64DivS:
      case Expr::I64DivU:
        return EmitDivOrMod(f, ValType::I64, IsDiv(true), IsUnsigned(op == Expr::I64DivU), def);
      case Expr::I64RemS:
      case Expr::I64RemU:
        return EmitDivOrMod(f, ValType::I64, IsDiv(false), IsUnsigned(op == Expr::I64RemU), def);

      // F32
      case Expr::F32Const:
        return EmitLiteral(f, ValType::F32, def);
      case Expr::F32Add:
        return EmitAddOrSub(f, ValType::F32, IsAdd(true), def);
      case Expr::F32Sub:
        return EmitAddOrSub(f, ValType::F32, IsAdd(false), def);
      case Expr::F32Mul:
        return EmitMultiply(f, ValType::F32, def);
      case Expr::F32Div:
        return EmitDivOrMod(f, ValType::F32, IsDiv(true), def);
      case Expr::F32Min:
        return EmitMathMinMax(f, ValType::F32, IsMax(false), def);
      case Expr::F32Max:
        return EmitMathMinMax(f, ValType::F32, IsMax(true), def);
      case Expr::F32Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F32, def);
      case Expr::F32Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F32, def);
      case Expr::F32Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F32, def);
      case Expr::F32Ceil:
      case Expr::F32Floor:
        return EmitF32MathBuiltinCall(f, exprOffset, op, def);
      case Expr::F32DemoteF64:
        return EmitUnary<MToFloat32>(f, def);
      case Expr::F32ConvertSI32:
        return EmitUnary<MToFloat32>(f, def);
      case Expr::F32ConvertUI32:
        return EmitUnary<MAsmJSUnsignedToFloat32>(f, def);
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F32,
                                             IsUnsigned(op == Expr::F32ConvertUI64), def);
      case Expr::F32Load:
        return EmitLoad(f, Scalar::Float32, def);
      case Expr::F32Store:
        return EmitStore(f, Scalar::Float32, def);
      case Expr::F32StoreF64:
        return EmitStoreWithCoercion(f, Scalar::Float32, Scalar::Float64, def);

      // F64
      case Expr::F64Const:
        return EmitLiteral(f, ValType::F64, def);
      case Expr::F64Add:
        return EmitAddOrSub(f, ValType::F64, IsAdd(true), def);
      case Expr::F64Sub:
        return EmitAddOrSub(f, ValType::F64, IsAdd(false), def);
      case Expr::F64Mul:
        return EmitMultiply(f, ValType::F64, def);
      case Expr::F64Div:
        return EmitDivOrMod(f, ValType::F64, IsDiv(true), def);
      case Expr::F64Mod:
        return EmitDivOrMod(f, ValType::F64, IsDiv(false), def);
      case Expr::F64Min:
        return EmitMathMinMax(f, ValType::F64, IsMax(false), def);
      case Expr::F64Max:
        return EmitMathMinMax(f, ValType::F64, IsMax(true), def);
      case Expr::F64Neg:
        return EmitUnaryWithType<MAsmJSNeg>(f, ValType::F64, def);
      case Expr::F64Abs:
        return EmitUnaryWithType<MAbs>(f, ValType::F64, def);
      case Expr::F64Sqrt:
        return EmitUnaryWithType<MSqrt>(f, ValType::F64, def);
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
        return EmitUnary<MToDouble>(f, def);
      case Expr::F64ConvertSI32:
        return EmitUnary<MToDouble>(f, def);
      case Expr::F64ConvertUI32:
        return EmitUnary<MAsmJSUnsignedToDouble>(f, def);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
        return EmitConvertI64ToFloatingPoint(f, ValType::F64,
                                             IsUnsigned(op == Expr::F64ConvertUI64), def);
      case Expr::F64Load:
        return EmitLoad(f, Scalar::Float64, def);
      case Expr::F64Store:
        return EmitStore(f, Scalar::Float64, def);
      case Expr::F64StoreF32:
        return EmitStoreWithCoercion(f, Scalar::Float64, Scalar::Float32, def);

      // SIMD
#define CASE(TYPE, OP, SIGN)                                                    \
      case Expr::TYPE##OP:                                                      \
        return EmitSimdOp(f, ValType::TYPE, SimdOperation::Fn_##OP, SIGN, def);
#define I32CASE(OP) CASE(I32x4, OP, SimdSign::Signed)
#define F32CASE(OP) CASE(F32x4, OP, SimdSign::NotApplicable)
#define B32CASE(OP) CASE(B32x4, OP, SimdSign::NotApplicable)
#define ENUMERATE(TYPE, FORALL, DO)                                             \
      case Expr::TYPE##Const:                                                   \
        return EmitLiteral(f, ValType::TYPE, def);                              \
      case Expr::TYPE##Constructor:                                             \
        return EmitSimdOp(f, ValType::TYPE, SimdOperation::Constructor,         \
                          SimdSign::NotApplicable, def);                        \
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
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_shiftRightByScalar,
                          SimdSign::Unsigned, def);
      case Expr::I32x4lessThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThan, SimdSign::Unsigned, def);
      case Expr::I32x4lessThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_lessThanOrEqual,
                          SimdSign::Unsigned, def);
      case Expr::I32x4greaterThanU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThan, SimdSign::Unsigned,
                          def);
      case Expr::I32x4greaterThanOrEqualU:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_greaterThanOrEqual,
                          SimdSign::Unsigned, def);
      case Expr::I32x4fromFloat32x4U:
        return EmitSimdOp(f, ValType::I32x4, SimdOperation::Fn_fromFloat32x4,
                          SimdSign::Unsigned, def);

      // Atomics
      case Expr::AtomicsFence:
        *def = nullptr;
        f.memoryBarrier(MembarFull);
        return true;
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

      // Future opcodes
      case Expr::F32CopySign:
      case Expr::F32Trunc:
      case Expr::F32Nearest:
      case Expr::F64CopySign:
      case Expr::F64Nearest:
      case Expr::F64Trunc:
      case Expr::I64ReinterpretF64:
      case Expr::F64ReinterpretI64:
      case Expr::I32ReinterpretF32:
      case Expr::F32ReinterpretI32:
      case Expr::I64Load8S:
      case Expr::I64Load16S:
      case Expr::I64Load32S:
      case Expr::I64Load8U:
      case Expr::I64Load16U:
      case Expr::I64Load32U:
      case Expr::I64Load:
      case Expr::I64Store8:
      case Expr::I64Store16:
      case Expr::I64Store32:
      case Expr::I64Store:
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
      case Expr::I64Eqz:
      case Expr::I32Rotr:
      case Expr::I32Rotl:
      case Expr::I64Rotr:
      case Expr::I64Rotl:
      case Expr::MemorySize:
      case Expr::GrowMemory:
        MOZ_CRASH("NYI");
        break;
      case Expr::Limit:;
    }

    MOZ_CRASH("unexpected wasm opcode");
}

bool
wasm::IonCompileFunction(IonCompileTask* task)
{
    int64_t before = PRMJ_Now();

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

    JitContext jitContext(CompileRuntime::get(task->runtime()), &results.alloc());
    const JitCompileOptions options;
    MIRGraph graph(&results.alloc());
    CompileInfo compileInfo(locals.length());
    MIRGenerator mir(nullptr, options, &results.alloc(), &graph, &compileInfo,
                     IonOptimizations.get(OptimizationLevel::AsmJS));
    mir.initUsesSignalHandlersForAsmJSOOB(task->mg().args().useSignalHandlersForOOB);
    mir.initMinAsmJSHeapLength(task->mg().minHeapLength());

    // Build MIR graph
    {
        FunctionCompiler f(task->mg(), d, func, locals, mir, results);
        if (!f.init())
            return false;

        MDefinition* last;
        while (!f.done()) {
            if (!EmitExpr(f, &last))
                return false;
        }

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
